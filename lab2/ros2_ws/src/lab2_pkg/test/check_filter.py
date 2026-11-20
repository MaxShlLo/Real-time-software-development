#!/usr/bin/env python3
"""Integration checks against the built C++ node; run inside the ROS container."""
import os
from pathlib import Path
import signal
import subprocess
import time

import rclpy
from ament_index_python.packages import get_package_prefix, get_package_share_directory
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


def wait_for(node, condition, description, timeout=12.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rclpy.spin_once(node, timeout_sec=0.05)
        if condition():
            return
    raise AssertionError(f'Timeout: {description}')


def spin_for(node, duration):
    deadline = time.monotonic() + duration
    while time.monotonic() < deadline:
        rclpy.spin_once(node, timeout_sec=0.05)


def stop_process(process):
    if process.poll() is not None:
        return
    if isinstance(process.args, list) and process.args[:2] == ['ros2', 'launch']:
        # launch forwards SIGINT to its children; signaling the entire group would
        # interrupt each C++ child twice and produce misleading shutdown errors.
        process.send_signal(signal.SIGINT)
    else:
        os.killpg(process.pid, signal.SIGINT)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)


def check_profile(node, publisher, override=False):
    executable = Path(get_package_prefix('lab2_pkg')) / 'lib/lab2_pkg/data_filter'
    command = [str(executable)]
    if override:
        config = Path(get_package_share_directory('lab2_pkg')) / 'config/qos_overrides.yaml'
        command += ['--ros-args', '--params-file', str(config)]
    received = []
    subscription = None
    incompatible = None
    process = subprocess.Popen(command, start_new_session=True)
    try:
        wait_for(node, lambda: len(node.get_publishers_info_by_topic('/uppercase_topic')) == 1,
                 'filter publisher discovery')
        assert process.poll() is None, 'Filter exited unexpectedly'
        spin_for(node, 0.5)
        assert publisher.get_subscription_count() == 0, 'Input subscribed without listeners'
        profile = node.get_publishers_info_by_topic('/uppercase_topic')[0].qos_profile
        expected_reliability = ReliabilityPolicy.BEST_EFFORT if override else ReliabilityPolicy.RELIABLE
        expected_durability = DurabilityPolicy.VOLATILE if override else DurabilityPolicy.TRANSIENT_LOCAL
        assert profile.reliability == expected_reliability, profile
        assert profile.durability == expected_durability, profile
        # DDS discovery may omit endpoint depth, so verify depth via the node's parameter below.
        print(f'PASS: {"override" if override else "default"} publisher reliability/durability', flush=True)

        if override:
            incompatible = node.create_subscription(
                String, '/uppercase_topic', lambda msg: None,
                QoSProfile(depth=1, reliability=ReliabilityPolicy.RELIABLE,
                           durability=DurabilityPolicy.VOLATILE))
            spin_for(node, 1.0)
            assert publisher.get_subscription_count() == 0, 'Incompatible listener enabled input'
            node.destroy_subscription(incompatible)
            incompatible = None
            print('PASS: incompatible QoS listener does not enable input', flush=True)

        output_qos = QoSProfile(depth=1, reliability=expected_reliability,
                                durability=expected_durability)
        subscription = node.create_subscription(
            String, '/uppercase_topic', lambda msg: received.append(msg.data), output_qos)
        wait_for(node, lambda: publisher.get_subscription_count() == 1, 'lazy input activation')
        print('PASS: input enabled after output listener connects', flush=True)

        examples = [('Hello ROS 2', 'HELLO ROS 2'),
                    ('Привіт, Україно! ґ є і ї', 'ПРИВІТ, УКРАЇНО! Ґ Є І Ї'),
                    ('', ''), ('last sample', 'LAST SAMPLE')]
        for original, expected in examples:
            received.clear()
            publisher.publish(String(data=original))
            wait_for(node, lambda: expected in received, f'uppercase of {original!r}')
        print('PASS: Latin, Ukrainian, empty and final messages', flush=True)

        node.destroy_subscription(subscription)
        subscription = None
        wait_for(node, lambda: publisher.get_subscription_count() == 0, 'lazy input deactivation')
        print('PASS: input disabled after last output listener disconnects', flush=True)

        received.clear()
        subscription = node.create_subscription(
            String, '/uppercase_topic', lambda msg: received.append(msg.data), output_qos)
        wait_for(node, lambda: publisher.get_subscription_count() == 1, 'late subscriber discovery')
        if override:
            spin_for(node, 0.7)
            assert not received, f'Volatile publisher replayed history: {received}'
            print('PASS: volatile profile does not replay history', flush=True)
        else:
            wait_for(node, lambda: bool(received), 'retained message')
            spin_for(node, 0.5)
            assert received == ['LAST SAMPLE'], f'Expected only last message: {received}'
            print('PASS: late listener receives exactly the last retained message', flush=True)

        from rcl_interfaces.srv import GetParameters
        client = node.create_client(GetParameters, '/data_filter/get_parameters')
        try:
            assert client.wait_for_service(timeout_sec=5)
            request = GetParameters.Request()
            request.names = ['qos_overrides./uppercase_topic.publisher.depth']
            future = client.call_async(request)
            wait_for(node, future.done, 'QoS depth parameter')
            assert future.result().values[0].integer_value == (3 if override else 1)
            print('PASS: configured depth is ' + ('3' if override else '1'), flush=True)
        finally:
            node.destroy_client(client)
    finally:
        if subscription is not None:
            node.destroy_subscription(subscription)
        if incompatible is not None:
            node.destroy_subscription(incompatible)
        stop_process(process)
        wait_for(node, lambda: not node.get_publishers_info_by_topic('/uppercase_topic'),
                 'filter shutdown')


def main():
    rclpy.init()
    node = rclpy.create_node('lab2_integration_check')
    publisher = node.create_publisher(String, '/input_topic', 10)
    try:
        check_profile(node, publisher)
        check_profile(node, publisher, override=True)
        print('ALL CHECKS PASSED', flush=True)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
