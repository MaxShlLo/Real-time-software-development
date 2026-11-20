#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "lab2_interfaces/action/process_array.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "rclcpp_components/register_node_macro.hpp"

namespace lab2_namespace
{
class ArrayProcessor : public rclcpp::Node
{
  using Action = lab2_interfaces::action::ProcessArray;
  using GoalHandle = rclcpp_action::ServerGoalHandle<Action>;

public:
  explicit ArrayProcessor(const rclcpp::NodeOptions & options)
  : Node("array_processor", options)
  {
    server_ = rclcpp_action::create_server<Action>(
      this, "process_array",
      [this](const rclcpp_action::GoalUUID &, std::shared_ptr<const Action::Goal> goal) {
        if (goal->data.empty() || goal->data.size() > 10000 || goal->delay_ms > 1000) {
          return rclcpp_action::GoalResponse::REJECT;
        }
        for (const auto value : goal->data) {
          if (!std::isfinite(value) || !std::isfinite(value * value)) {
            return rclcpp_action::GoalResponse::REJECT;
          }
        }
        bool expected = false;
        if (!busy_.compare_exchange_strong(expected, true)) {
          return rclcpp_action::GoalResponse::REJECT;
        }
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [this](const std::shared_ptr<GoalHandle>) {
        wakeup_.notify_all();
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [this](const std::shared_ptr<GoalHandle> handle) {
        if (worker_.joinable()) {
          worker_.join();
        }
        // Приєднуємо попередній робочий потік.
        worker_ = std::thread([this, handle]() {execute(handle);});
      });
    RCLCPP_INFO(get_logger(), "Action /process_array ready: square each array element");
  }

  ~ArrayProcessor() override
  {
    stopping_.store(true);
    wakeup_.notify_all();
    if (worker_.joinable()) {
      worker_.join();
    }
  }

private:
  void execute(const std::shared_ptr<GoalHandle> handle)
  {
    struct ReleaseBusy
    {
      std::atomic<bool> & busy;
      ~ReleaseBusy() {busy.store(false);}
    } release{busy_};
    const auto goal = handle->get_goal();
    auto result = std::make_shared<Action::Result>();
    auto feedback = std::make_shared<Action::Feedback>();
    result->processed_data.reserve(goal->data.size());
    feedback->total_count = static_cast<uint32_t>(goal->data.size());
    try {
      for (const auto value : goal->data) {
        // Перевіряємо скасування під час затримки.
        const auto deadline = std::chrono::steady_clock::now() +
          std::chrono::milliseconds(goal->delay_ms);
        while (std::chrono::steady_clock::now() < deadline && rclcpp::ok() &&
          !stopping_.load() && !handle->is_canceling())
        {
          std::unique_lock<std::mutex> lock(wait_mutex_);
          wakeup_.wait_for(lock, std::chrono::milliseconds(10));
        }
        if (stopping_.load() || !rclcpp::ok()) {
          return;
        }
        if (handle->is_canceling()) {
          handle->canceled(result);
          RCLCPP_INFO(get_logger(), "Action canceled after %u elements", result->processed_count);
          return;
        }
        result->processed_data.push_back(value * value);
        result->processed_count = static_cast<uint32_t>(result->processed_data.size());
        feedback->processed_count = result->processed_count;
        feedback->progress = 100.0F * feedback->processed_count / feedback->total_count;
        handle->publish_feedback(feedback);
        RCLCPP_INFO(get_logger(), "Progress: %u/%u (%.1f%%)",
          feedback->processed_count, feedback->total_count, feedback->progress);
      }
      if (handle->is_canceling()) {
        handle->canceled(result);
      } else {
        handle->succeed(result);
        RCLCPP_INFO(get_logger(), "Action SUCCEEDED: %u elements", result->processed_count);
      }
    } catch (const std::exception & error) {
      RCLCPP_ERROR(get_logger(), "Action failed: %s", error.what());
      if (rclcpp::ok() && handle->is_active()) {
        handle->abort(result);
      }
    }
  }

  rclcpp_action::Server<Action>::SharedPtr server_;
  std::atomic<bool> busy_{false};
  std::atomic<bool> stopping_{false};
  std::mutex wait_mutex_;
  std::condition_variable wakeup_;
  std::thread worker_;
};
}  // namespace lab2_namespace

RCLCPP_COMPONENTS_REGISTER_NODE(lab2_namespace::ArrayProcessor)
