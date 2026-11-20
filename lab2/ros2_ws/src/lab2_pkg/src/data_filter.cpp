#include <codecvt>
#include <chrono>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "rcl/service_introspection.h"
#include "lab2_interfaces/srv/reset_filter.hpp"

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/qos_overriding_options.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "std_msgs/msg/string.hpp"

namespace lab2_namespace
{
class DataFilter : public rclcpp::Node
{
public:
  explicit DataFilter(const rclcpp::NodeOptions & options)
  : Node("data_filter", options), text_locale_("C.UTF-8")
  {
    input_topic_ = declare_parameter<std::string>("input_topic", "input_topic");
    const auto output_topic = declare_parameter<std::string>("output_topic", "uppercase_topic");

    rclcpp::PublisherOptions publisher_options;
    publisher_options.qos_overriding_options = rclcpp::QosOverridingOptions{
      rclcpp::QosPolicyKind::Depth,
      rclcpp::QosPolicyKind::Reliability,
      rclcpp::QosPolicyKind::Durability};

    // Спільна взаємовиключна група callback-ів.
    publisher_options.event_callbacks.matched_callback =
      [this](rclcpp::MatchedInfo & info) {
        if (info.current_count > 0 && !subscription_) {
          subscription_ = create_subscription<std_msgs::msg::String>(
            input_topic_, rclcpp::QoS(10),
            [this](std_msgs::msg::String::ConstSharedPtr message) {
              process_message(*message);
            });
          RCLCPP_INFO(get_logger(), "Input subscription enabled: output has listeners.");
        } else if (info.current_count == 0 && subscription_) {
          subscription_.reset();
          RCLCPP_INFO(get_logger(), "Input subscription disabled: no output listeners.");
        }
      };

    publisher_ = create_publisher<std_msgs::msg::String>(
      output_topic, rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      publisher_options);

    // Затримка для перевірки таймауту.
    const auto reset_delay = declare_parameter<int>("reset_delay_ms", 0);
    if (reset_delay < 0 || reset_delay > 5000) {
      throw std::invalid_argument("reset_delay_ms must be between 0 and 5000");
    }
    reset_service_ = create_service<lab2_interfaces::srv::ResetFilter>(
      "reset_filter",
      [this, reset_delay](
        const std::shared_ptr<lab2_interfaces::srv::ResetFilter::Request>,
        std::shared_ptr<lab2_interfaces::srv::ResetFilter::Response> response) {
        if (reset_delay > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(reset_delay));
        }
        response->previous_count = processed_count_;
        processed_count_ = 0;
        response->success = true;
        response->message = "Processed message counter reset to zero";
        RCLCPP_INFO(get_logger(), "ResetFilter: previous_count=%llu, current_count=0",
          static_cast<unsigned long long>(response->previous_count));
      });
    reset_service_->configure_introspection(
      get_clock(), rclcpp::SystemDefaultsQoS(), RCL_SERVICE_INTROSPECTION_CONTENTS);
    RCLCPP_INFO(
      get_logger(), "Ready: %s -> %s. Waiting for output listeners.",
      input_topic_.c_str(), publisher_->get_topic_name());
  }

private:
  std::wstring decode_utf8(const std::string & input) const
  {
    if (input.empty()) {
      return {};
    }
    const auto & converter = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>>(
      text_locale_);
    std::mbstate_t state{};
    std::wstring output(input.size(), L'\0');
    const char * next_input = nullptr;
    wchar_t * next_output = nullptr;
    const auto result = converter.in(
      state, input.data(), input.data() + input.size(), next_input,
      output.data(), output.data() + output.size(), next_output);
    if (result != std::codecvt_base::ok || next_input != input.data() + input.size()) {
      throw std::range_error("invalid UTF-8 sequence");
    }
    output.resize(static_cast<std::size_t>(next_output - output.data()));
    return output;
  }

  std::string encode_utf8(const std::wstring & input) const
  {
    if (input.empty()) {
      return {};
    }
    const auto & converter = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>>(
      text_locale_);
    std::mbstate_t state{};
    std::string output(input.size() * converter.max_length(), '\0');
    const wchar_t * next_input = nullptr;
    char * next_output = nullptr;
    const auto result = converter.out(
      state, input.data(), input.data() + input.size(), next_input,
      output.data(), output.data() + output.size(), next_output);
    if (result != std::codecvt_base::ok || next_input != input.data() + input.size()) {
      throw std::range_error("cannot encode text as UTF-8");
    }
    output.resize(static_cast<std::size_t>(next_output - output.data()));
    return output;
  }

  void process_message(const std_msgs::msg::String & input)
  {
    // Перетворення українських літер через UTF-8.
    try {
      auto text = decode_utf8(input.data);
      if (!text.empty()) {
        std::use_facet<std::ctype<wchar_t>>(text_locale_).toupper(
          &text[0], &text[0] + text.size());
      }
      std_msgs::msg::String output;
      output.data = encode_utf8(text);
      RCLCPP_INFO(get_logger(), "Converted: '%s' -> '%s'", input.data.c_str(), output.data.c_str());
      publisher_->publish(output);
      ++processed_count_;
    } catch (const std::range_error & error) {
      RCLCPP_WARN(get_logger(), "Skipped invalid UTF-8 text: %s", error.what());
    }
  }

  std::locale text_locale_;
  std::string input_topic_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  // Сервіс і підписка в одній групі.
  uint64_t processed_count_{0};
  rclcpp::Service<lab2_interfaces::srv::ResetFilter>::SharedPtr reset_service_;
};
}  // namespace lab2_namespace

RCLCPP_COMPONENTS_REGISTER_NODE(lab2_namespace::DataFilter)
