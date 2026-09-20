#!/bin/bash
set -e
# Сорсинг середовища ROS 2, як вимагається у завданні
source /opt/ros/$ROS_DISTRO/setup.bash
exec "$@"