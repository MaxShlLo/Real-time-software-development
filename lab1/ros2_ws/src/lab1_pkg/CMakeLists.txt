cmake_minimum_required(VERSION 3.20)
project(lab1_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)

add_library(component_node SHARED src/component_node.cpp)

rclcpp_components_register_nodes(component_node "lab1_namespace::ComponentNode")

# add_executable(component_node src/component_node.cpp)
target_include_directories(component_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_compile_features(component_node PUBLIC c_std_17 cxx_std_20)
target_link_libraries(
    component_node
    rclcpp::rclcpp
    rclcpp_components::component
)

add_executable(lifecycle_nodes src/lifecycle_nodes.cpp)

target_link_libraries(
    lifecycle_nodes
    rclcpp::rclcpp
    rclcpp_lifecycle::rclcpp_lifecycle
)

install(TARGETS
  component_node
  lifecycle_nodes
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
