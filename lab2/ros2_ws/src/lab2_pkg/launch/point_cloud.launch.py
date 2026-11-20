from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    intra = ParameterValue(LaunchConfiguration('intra_process'), value_type=bool)
    return LaunchDescription([
        DeclareLaunchArgument('intra_process', default_value='true'),
        ComposableNodeContainer(
            name='cloud_container', namespace='',
            package='rclcpp_components', executable='component_container',
            output='screen',
            composable_node_descriptions=[
                ComposableNode(
                    package='lab2_pkg', plugin='lab2_namespace::CloudPublisher',
                    parameters=[{'point_count': 1000, 'period_ms': 500}],
                    extra_arguments=[{'use_intra_process_comms': intra}]),
                ComposableNode(
                    package='lab2_pkg', plugin='lab2_namespace::CloudSubscriber',
                    extra_arguments=[{'use_intra_process_comms': intra}]),
            ]),
    ])
