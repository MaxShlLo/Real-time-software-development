# Базовий офіційний образ ROS 2 Lyrical Luth
FROM ros:lyrical

# Встановлення colcon для збирання пакетів
RUN apt-get update && apt-get install -y \
    python3-colcon-common-extensions \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Встановлення робочої директорії
WORKDIR /ros2_ws

# Копіювання скрипта ініціалізації та надання прав на виконання
COPY entrypoint.sh /entrypoint.sh
RUN sed -i 's/\r$//' /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["bash"]