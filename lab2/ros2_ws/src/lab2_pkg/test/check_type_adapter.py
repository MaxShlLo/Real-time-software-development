#!/usr/bin/env python3
"""Prove custom-type delivery, pointer identity and ROS fallback using real components."""
import os
from pathlib import Path
import re
import struct
import subprocess

import rclpy
from sensor_msgs.msg import PointCloud2

from check_filter import stop_process, wait_for


RECORD = re.compile(
    r'CLOUD_(PUB|SUB) seq=(\d+) points=(\d+) object=(0x[0-9a-f]+) '
    r'data=(0x[0-9a-f]+)(?: valid=(true|false))? to_ros=(\d+) from_ros=(\d+)')


def records(path):
    return [dict(kind=m[0], seq=int(m[1]), points=int(m[2]), object=m[3], data=m[4],
                 valid=m[5], to_ros=int(m[6]), from_ros=int(m[7]))
            for m in RECORD.findall(path.read_text(encoding='utf-8', errors='replace'))]


def received(path):
    return [r for r in records(path) if r['kind'] == 'SUB']


def verify_same_addresses(path):
    publishers = {r['seq']: r for r in records(path) if r['kind'] == 'PUB'}
    subscribers = received(path)
    assert len(subscribers) >= 3
    for message in subscribers:
        assert message['valid'] == 'true' and message['points'] == 1000, message
        original = publishers[message['seq']]
        assert message['object'] == original['object'], (original, message)
        assert message['data'] == original['data'], (original, message)


def verify_ros_message(message):
    assert message.width == 1000 and message.height == 1
    assert message.header.frame_id == 'lab2_cloud'
    assert message.point_step >= 12 and len(message.data) >= message.row_step
    offsets = {f.name: f.offset for f in message.fields}
    endian = '>' if message.is_bigendian else '<'
    def xyz(index):
        return tuple(struct.unpack_from(endian + 'f', message.data,
                                        index * message.point_step + offsets[k])[0]
                     for k in ('x', 'y', 'z'))
    first, last = xyz(0), xyz(999)
    assert first[1] == 0.0 and first[2] == first[0], first
    assert last == (first[0], 999.0, first[0] + 499.5), last


def check_mode(node, intra_process, observer=False):
    name = 'cloud_external' if observer else ('cloud_intra' if intra_process else 'cloud_dds')
    log_dir = Path('/ros2_ws/log/verification')
    log_dir.mkdir(parents=True, exist_ok=True)
    path = log_dir / (name + '.log')
    output = path.open('w', encoding='utf-8')
    env = dict(os.environ, RCUTILS_COLORIZED_OUTPUT='0', RCUTILS_LOGGING_BUFFERED_STREAM='0')
    process = subprocess.Popen([
        'ros2', 'launch', 'lab2_pkg', 'point_cloud.launch.py',
        'intra_process:=' + str(intra_process).lower()],
        stdout=output, stderr=subprocess.STDOUT, env=env, start_new_session=True)
    listener = None
    try:
        wait_for(node, lambda: len(received(path)) >= 4, 'four PCL clouds: ' + name, timeout=30)
        assert process.poll() is None, path.read_text()
        assert all(r['valid'] == 'true' and r['points'] == 1000 for r in received(path))
        if intra_process:
            verify_same_addresses(path)
            assert all(r['to_ros'] == r['from_ros'] == 0 for r in records(path))
            assert 'ADAPTER ' not in path.read_text()
            print('PASS: intra-process PCL delivery; identical object/buffer addresses; zero conversions',
                  flush=True)
        else:
            assert all(r['to_ros'] > 0 and r['from_ros'] > 0 for r in received(path))
            print('PASS: disabled intra-process uses both adapter conversions; all XYZ values valid',
                  flush=True)

        if observer:
            messages = []
            listener = node.create_subscription(PointCloud2, '/point_cloud', messages.append, 10)
            wait_for(node, lambda: len(messages) >= 3, 'external PointCloud2 observer', timeout=15)
            for message in messages:
                verify_ros_message(message)
            assert any(r['to_ros'] > 0 for r in received(path))
            print('PASS: external PointCloud2 listener receives valid serialized XYZ data',
                  flush=True)
        print('Evidence:', path, flush=True)
    except Exception:
        print(path.read_text(encoding='utf-8', errors='replace'), flush=True)
        raise
    finally:
        if listener is not None:
            node.destroy_subscription(listener)
        stop_process(process)
        output.close()
        wait_for(node, lambda: not node.get_publishers_info_by_topic('/point_cloud'),
                 'cloud publisher shutdown')


def main():
    rclpy.init()
    node = rclpy.create_node('lab2_type_adapter_check')
    try:
        check_mode(node, True)
        check_mode(node, False)
        check_mode(node, False, observer=True)
        print('ALL TYPE ADAPTER CHECKS PASSED', flush=True)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
