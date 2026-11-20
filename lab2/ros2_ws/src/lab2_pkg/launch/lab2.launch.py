from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('intra_process', default_value='true'),
        Node(package='lab2_pkg', executable='data_filter', output='screen'),
        Node(package='lab2_pkg', executable='array_processor', output='screen'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(PathJoinSubstitution([
                FindPackageShare('lab2_pkg'), 'launch', 'point_cloud.launch.py'])),
            launch_arguments={
                'intra_process': LaunchConfiguration('intra_process'),
            }.items()),
    ])
