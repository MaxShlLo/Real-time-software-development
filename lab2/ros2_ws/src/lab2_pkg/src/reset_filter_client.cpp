#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include "lab2_interfaces/srv/reset_filter.hpp"
#include "rcl/service_introspection.h"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("reset_filter_client");
  const auto timeout_ms = node->declare_parameter<int>("timeout_ms", 2000);
  const auto service_wait_ms = node->declare_parameter<int>("service_wait_ms", 3000);
  const auto service_name = node->declare_parameter<std::string>("service_name", "reset_filter");
  if (timeout_ms < 1 || timeout_ms > 60000 || service_wait_ms < 1 || service_wait_ms > 60000) {
    RCLCPP_ERROR(node->get_logger(), "Timeout parameters must be between 1 and 60000 ms");
    rclcpp::shutdown();
    return 1;
  }
  auto client = node->create_client<lab2_interfaces::srv::ResetFilter>(service_name);
  client->configure_introspection(
    node->get_clock(), rclcpp::SystemDefaultsQoS(), RCL_SERVICE_INTROSPECTION_CONTENTS);
  if (!client->wait_for_service(std::chrono::milliseconds(service_wait_ms))) {
    RCLCPP_ERROR(node->get_logger(), "Service unavailable within %d ms",
      static_cast<int>(service_wait_ms));
    rclcpp::shutdown();
    return 2;
  }

  // Окремий потік обробляє відповідь.
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  std::thread spin_thread([&executor]() {executor.spin();});
  int exit_code = 0;
  try {
    auto request = std::make_shared<lab2_interfaces::srv::ResetFilter::Request>();
    auto future = client->async_send_request(request);
    if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::ready) {
      const auto response = future.get();
      RCLCPP_INFO(node->get_logger(), "success=%s previous_count=%llu message=%s",
        response->success ? "true" : "false",
        static_cast<unsigned long long>(response->previous_count), response->message.c_str());
      exit_code = response->success ? 0 : 1;
    } else {
      client->remove_pending_request(future);
      RCLCPP_ERROR(node->get_logger(),
        "Response timeout after %d ms; pending request removed", static_cast<int>(timeout_ms));
      exit_code = 3;
    }
  } catch (const std::exception & error) {
    RCLCPP_ERROR(node->get_logger(), "Service request failed: %s", error.what());
    exit_code = 1;
  }
  executor.cancel();
  spin_thread.join();
  rclcpp::shutdown();
  return exit_code;
}
