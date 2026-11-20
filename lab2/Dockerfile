FROM ros:lyrical

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-colcon-common-extensions \
    build-essential \
    ros-lyrical-rclcpp-components \
    ros-lyrical-std-msgs \
    ros-lyrical-rclcpp-action \
    ros-lyrical-rosidl-default-generators \
    ros-lyrical-launch-ros \
    ros-lyrical-sensor-msgs \
    libeigen3-dev \
    libboost-dev \
    # Встановлюємо заголовки PCL.
    && mkdir -p /tmp/pcl-headers \
    && cd /tmp/pcl-headers \
    && apt-get download libpcl-dev \
    && dpkg-deb -x libpcl-dev_*.deb extracted \
    && cp -a extracted/usr/include/. /usr/include/ \
    && rm -rf /tmp/pcl-headers \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ros2_ws

COPY entrypoint.sh /entrypoint.sh
RUN sed -i 's/\r$//' /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]
