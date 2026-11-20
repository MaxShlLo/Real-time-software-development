#!/usr/bin/env python3
"""End-to-end checks of the C++ service/client, introspection and action worker."""
from pathlib import Path
import subprocess
import time

from action_msgs.msg import GoalStatus
from ament_index_python.packages import get_package_prefix
import rclpy
from rclpy.action import ActionClient
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String

from lab2_interfaces.action import ProcessArray
from lab2_interfaces.srv import ResetFilter, ResetFilter_Event
from check_filter import spin_for, stop_process, wait_for


def result_of(node, future, description):
    wait_for(node, future.done, description)
    return future.result()


def reset_with_cpp(arguments=(), expected=0, node=None):
    executable = Path(get_package_prefix('lab2_pkg')) / 'lib/lab2_pkg/reset_filter_client'
    if node is None:
        result = subprocess.run([str(executable), *arguments], capture_output=True,
                                text=True, encoding='utf-8', timeout=10)
    else:
        process = subprocess.Popen([str(executable), *arguments], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, encoding='utf-8')
        try:
            # Drain event callbacks while the short-lived C++ introspection writer exists.
            wait_for(node, lambda: process.poll() is not None, 'C++ client process', timeout=10)
            stdout, stderr = process.communicate(timeout=2)
            result = subprocess.CompletedProcess(process.args, process.returncode, stdout, stderr)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=2)
    print(result.stdout + result.stderr, end='', flush=True)
    assert result.returncode == expected, result
    return result


def check_service(node):
    prefix = Path(get_package_prefix('lab2_pkg')) / 'lib/lab2_pkg'
    process = subprocess.Popen([str(prefix / 'data_filter')], start_new_session=True)
    output = []
    events = []
    source = node.create_publisher(String, '/input_topic', 10)
    listener = node.create_subscription(
        String, '/uppercase_topic', lambda msg: output.append(msg.data),
        QoSProfile(depth=1, durability=DurabilityPolicy.TRANSIENT_LOCAL,
                   reliability=ReliabilityPolicy.RELIABLE))
    event_sub = node.create_subscription(
        ResetFilter_Event, '/reset_filter/_service_event', lambda msg: events.append(msg), 100)
    client = node.create_client(ResetFilter, '/reset_filter')
    try:
        assert client.wait_for_service(timeout_sec=10)
        wait_for(node, lambda: source.get_subscription_count() == 1, 'filter input')
        # Allow the hidden event subscription to match both C++ introspection publishers.
        spin_for(node, 0.5)
        reset_with_cpp(node=node)
        wait_for(node, lambda: {0, 1, 2, 3}.issubset({e.info.event_type for e in events}),
                 'request sent/received and response sent/received introspection')
        assert any(e.response and e.response[0].success for e in events)
        print('PASS: C++ async client and all four CONTENTS introspection event types', flush=True)

        for value in ['one', 'два', 'three']:
            output.clear()
            source.publish(String(data=value))
            wait_for(node, lambda: value.upper() in output, 'processed text before reset')
        response = result_of(node, client.call_async(ResetFilter.Request()), 'reset count')
        assert response.success and response.previous_count == 3, response
        response = result_of(node, client.call_async(ResetFilter.Request()), 'reset again')
        assert response.previous_count == 0, response
        print('PASS: ResetFilter returns previous_count=3, then 0', flush=True)

        listing = subprocess.run(['ros2', 'topic', 'list', '--include-hidden-topics'],
                                 capture_output=True, text=True, timeout=10, check=True)
        assert '/reset_filter/_service_event' in listing.stdout
        echo = subprocess.Popen(
            ['ros2', 'topic', 'echo', '/reset_filter/_service_event',
             'lab2_interfaces/srv/ResetFilter_Event', '--once'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
        try:
            wait_for(node, lambda: echo.poll() is not None or
                     node.count_subscribers('/reset_filter/_service_event') >= 2,
                     'CLI hidden event listener')
            # --once may already have printed a retained event and exited before
            # graph discovery observes its subscription. That is successful CLI output.
            if echo.poll() is None:
                result_of(node, client.call_async(ResetFilter.Request()), 'reset for CLI echo')
            stdout, stderr = echo.communicate(timeout=10)
            assert echo.returncode == 0 and 'event_type:' in stdout, stdout + stderr
            print('PASS: CLI --include-hidden-topics and echo of _service_event', flush=True)
        finally:
            if echo.poll() is None:
                echo.terminate()
                echo.wait(timeout=5)

        reset_with_cpp(['--ros-args', '-p', 'service_name:=/missing_reset_filter',
                        '-p', 'service_wait_ms:=200'], expected=2)
        print('PASS: unavailable service is bounded by discovery timeout (exit 2)', flush=True)
    finally:
        node.destroy_subscription(listener)
        node.destroy_subscription(event_sub)
        node.destroy_publisher(source)
        node.destroy_client(client)
        stop_process(process)
        wait_for(node, lambda: not node.get_publishers_info_by_topic('/uppercase_topic'),
                 'filter shutdown')

    slow = subprocess.Popen([str(prefix / 'data_filter'), '--ros-args',
                             '-p', 'reset_delay_ms:=350'], start_new_session=True)
    client = node.create_client(ResetFilter, '/reset_filter')
    try:
        assert client.wait_for_service(timeout_sec=10)
        reset_with_cpp(['--ros-args', '-p', 'timeout_ms:=50'], expected=3)
        reset_with_cpp(['--ros-args', '-p', 'timeout_ms:=1500'])
        print('PASS: response timeout (exit 3), cleanup and later successful request', flush=True)
    finally:
        node.destroy_client(client)
        stop_process(slow)


def check_action(node):
    executable = Path(get_package_prefix('lab2_pkg')) / 'lib/lab2_pkg/array_processor'
    process = subprocess.Popen([str(executable)], start_new_session=True)
    client = ActionClient(node, ProcessArray, '/process_array')
    try:
        assert client.wait_for_server(timeout_sec=10)
        feedback = []
        goal = ProcessArray.Goal(data=[1.0, -2.0, 3.5], delay_ms=30)
        handle = result_of(node, client.send_goal_async(
            goal, feedback_callback=lambda msg: feedback.append(msg.feedback)), 'accept action')
        assert handle.accepted
        response = result_of(node, handle.get_result_async(), 'action result')
        assert response.status == GoalStatus.STATUS_SUCCEEDED
        assert list(response.result.processed_data) == [1.0, 4.0, 12.25]
        assert response.result.processed_count == 3
        wait_for(node, lambda: feedback and feedback[-1].progress == 100.0, '100% feedback')
        assert [f.processed_count for f in feedback] == [1, 2, 3]
        print('PASS: Action result [1,4,12.25], ordered feedback and SUCCEEDED', flush=True)

        for values in [[], [float('nan')], [1e300]]:
            handle = result_of(node, client.send_goal_async(
                ProcessArray.Goal(data=values, delay_ms=0)), 'reject invalid goal')
            assert not handle.accepted
        print('PASS: empty, nonfinite and overflowing goals rejected', flush=True)

        feedback.clear()
        handle = result_of(node, client.send_goal_async(
            ProcessArray.Goal(data=[2.0] * 20, delay_ms=100),
            feedback_callback=lambda msg: feedback.append(msg.feedback)), 'long goal')
        assert handle.accepted
        wait_for(node, lambda: bool(feedback), 'first long-task feedback')
        other = result_of(node, client.send_goal_async(
            ProcessArray.Goal(data=[1.0], delay_ms=0)), 'reject busy goal')
        assert not other.accepted
        cancel = result_of(node, handle.cancel_goal_async(), 'cancel response')
        assert cancel.goals_canceling
        response = result_of(node, handle.get_result_async(), 'canceled result')
        assert response.status == GoalStatus.STATUS_CANCELED
        assert 0 < response.result.processed_count < 20
        assert len(response.result.processed_data) == response.result.processed_count
        print('PASS: busy goal rejected; cancel returns partial result and CANCELED', flush=True)

        again = result_of(node, client.send_goal_async(
            ProcessArray.Goal(data=[3.0], delay_ms=0)), 'accept after cancellation')
        assert again.accepted
        response = result_of(node, again.get_result_async(), 'result after cancellation')
        assert response.status == GoalStatus.STATUS_SUCCEEDED
        assert list(response.result.processed_data) == [9.0]
        print('PASS: server accepts and completes a new goal after cancellation', flush=True)

        active = result_of(node, client.send_goal_async(
            ProcessArray.Goal(data=[1.0] * 100, delay_ms=1000)), 'goal before shutdown')
        assert active.accepted
        started = time.monotonic()
        stop_process(process)
        assert process.returncode == 0 and time.monotonic() - started < 5
        print('PASS: shutdown joins active worker without hanging', flush=True)
    finally:
        client.destroy()
        if process.poll() is None:
            stop_process(process)


def main():
    rclpy.init()
    node = rclpy.create_node('lab2_service_action_check')
    try:
        check_service(node)
        check_action(node)
        print('ALL SERVICE AND ACTION CHECKS PASSED', flush=True)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
