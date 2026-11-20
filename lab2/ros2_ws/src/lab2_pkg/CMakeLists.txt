cmake_minimum_required(VERSION 3.20)
project(lab2_pkg)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)
find_package(rcl REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(lab2_interfaces REQUIRED)
find_package(Threads REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(Eigen3 REQUIRED)
# Заголовки типів PCL.
file(GLOB PCL_HEADER_ROOTS "/usr/include/pcl-*")
find_path(PCL_TYPES_INCLUDE_DIR pcl/point_cloud.h PATHS ${PCL_HEADER_ROOTS} REQUIRED)

add_library(data_filter_component SHARED src/data_filter.cpp)
target_compile_features(data_filter_component PUBLIC cxx_std_17)
target_compile_options(data_filter_component PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(data_filter_component
  rclcpp::rclcpp
  rclcpp_components::component
  rcl::rcl
  Threads::Threads
  ${lab2_interfaces_TARGETS}
  ${std_msgs_TARGETS})

# Компонент і виконуваний файл.
rclcpp_components_register_node(data_filter_component
  PLUGIN "lab2_namespace::DataFilter"
  EXECUTABLE data_filter)

add_executable(reset_filter_client src/reset_filter_client.cpp)
target_compile_features(reset_filter_client PUBLIC cxx_std_17)
target_compile_options(reset_filter_client PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(reset_filter_client
  rclcpp::rclcpp rcl::rcl Threads::Threads ${lab2_interfaces_TARGETS})

add_library(array_processor_component SHARED src/array_processor.cpp)
target_compile_features(array_processor_component PUBLIC cxx_std_17)
target_compile_options(array_processor_component PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(array_processor_component
  rclcpp::rclcpp rclcpp_components::component rclcpp_action::rclcpp_action
  Threads::Threads ${lab2_interfaces_TARGETS})
rclcpp_components_register_node(array_processor_component
  PLUGIN "lab2_namespace::ArrayProcessor"
  EXECUTABLE array_processor)

add_library(point_cloud_components SHARED src/point_cloud_components.cpp)
target_compile_features(point_cloud_components PUBLIC cxx_std_17)
target_compile_options(point_cloud_components PRIVATE -Wall -Wextra)
target_include_directories(point_cloud_components PRIVATE include)
target_include_directories(point_cloud_components SYSTEM PRIVATE ${PCL_TYPES_INCLUDE_DIR})
target_link_libraries(point_cloud_components
  rclcpp::rclcpp rclcpp_components::component Eigen3::Eigen ${sensor_msgs_TARGETS})
rclcpp_components_register_nodes(point_cloud_components
  "lab2_namespace::CloudPublisher" "lab2_namespace::CloudSubscriber")

install(TARGETS data_filter_component array_processor_component point_cloud_components
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(TARGETS reset_filter_client DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_package()
