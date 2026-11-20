#!/usr/bin/env python3
"""Verify the shared launch file and the exact CLI commands used in the report."""
import subprocess
import rclpy
from check_filter import stop_process, wait_for


def cli(*arguments):
    result = subprocess.run(['ros2', *arguments], capture_output=True, text=True,
                            encoding='utf-8', timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, flush=True)
    return result.stdout


def main():
    rclpy.init()
    node = rclpy.create_node('lab2_launch_check')
    process = subprocess.Popen(['ros2', 'launch', 'lab2_pkg', 'lab2.launch.py'],
                               start_new_session=True)
    goal = None
    try:
        wait_for(node, lambda: {
            'data_filter', 'array_processor', 'cloud_publisher', 'cloud_subscriber'
        }.issubset(set(node.get_node_names())), 'all four launched nodes', timeout=20)
        response = cli('topic', 'echo', '/point_cloud', 'sensor_msgs/msg/PointCloud2',
                       '--field', 'width', '--once')
        assert '1000' in response
        print('PASS: full launch includes both PCL components; CLI reads PointCloud2 width=1000',
              flush=True)
        response = cli('service', 'call', '/reset_filter',
                       'lab2_interfaces/srv/ResetFilter', '{}')
        assert 'success=True' in response
        response = cli('action', 'send_goal', '/process_array',
                       'lab2_interfaces/action/ProcessArray',
                       '{data: [1.0, -2.0, 3.5], delay_ms: 30}', '--feedback')
        assert 'SUCCEEDED' in response and '12.25' in response
        print('PASS: shared launch and CLI reset/action commands', flush=True)

        goal = subprocess.Popen(
            ['ros2', 'action', 'send_goal', '/process_array',
             'lab2_interfaces/action/ProcessArray',
             '{data: [1.0, 2.0, 3.0, 4.0, 5.0], delay_ms: 1000}', '--feedback'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
        # Wait for goal acceptance before requesting cancellation through the CLI service.
        lines = []
        for line in goal.stdout:
            lines.append(line)
            if 'Goal accepted' in line:
                break
        assert any('Goal accepted' in line for line in lines), ''.join(lines)
        response = cli('service', 'call', '/process_array/_action/cancel_goal',
                       'action_msgs/srv/CancelGoal',
                       '{goal_info: {goal_id: {uuid: [0, 0, 0, 0, 0, 0, 0, 0, '
                       '0, 0, 0, 0, 0, 0, 0, 0]}, stamp: {sec: 0, nanosec: 0}}}')
        assert 'return_code=0' in response
        stdout, stderr = goal.communicate(timeout=10)
        assert goal.returncode == 0 and 'CANCELED' in stdout, stdout + stderr
        print('PASS: CLI cancellation of active goal returns CANCELED', flush=True)
        print('ALL LAUNCH AND CLI CHECKS PASSED', flush=True)
    finally:
        if goal is not None and goal.poll() is None:
            goal.terminate()
            goal.wait(timeout=5)
        stop_process(process)
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
