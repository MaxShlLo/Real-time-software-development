#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "lab2_pkg/point_cloud_adapter.hpp"
#include "rclcpp_components/register_node_macro.hpp"

namespace lab2_namespace
{
using AdaptedCloud = rclcpp::TypeAdapter<PointCloud, sensor_msgs::msg::PointCloud2>;

class CloudPublisher : public rclcpp::Node
{
public:
  explicit CloudPublisher(const rclcpp::NodeOptions & options)
  : Node("cloud_publisher", options)
  {
    point_count_ = declare_parameter<int>("point_count", 1000);
    const auto period_ms = declare_parameter<int>("period_ms", 500);
    if (point_count_ < 1 || point_count_ > 1000000 || period_ms < 20 || period_ms > 60000) {
      throw std::invalid_argument("point_count must be 1..1000000; period_ms must be 20..60000");
    }
    publisher_ = create_publisher<AdaptedCloud>("point_cloud", rclcpp::QoS(1).reliable());
    timer_ = create_wall_timer(std::chrono::milliseconds(period_ms), [this]() {publish_cloud();});
    RCLCPP_INFO(get_logger(), "Cloud publisher ready; intra_process=%s",
      options.use_intra_process_comms() ? "true" : "false");
  }

private:
  void publish_cloud()
  {
    // Генеруємо хмару за наявності слухача.
    if (publisher_->get_subscription_count() == 0) {
      return;
    }
    auto cloud = std::make_unique<PointCloud>();
    cloud->resize(static_cast<std::size_t>(point_count_));
    cloud->header.frame_id = "lab2_cloud";
    cloud->header.stamp = static_cast<uint64_t>(now().nanoseconds() / 1000);
    ++sequence_;
    for (std::size_t i = 0; i < cloud->size(); ++i) {
      cloud->points[i].x = static_cast<float>(sequence_);
      cloud->points[i].y = static_cast<float>(i);
      cloud->points[i].z = static_cast<float>(sequence_) + static_cast<float>(i) * 0.5F;
    }
    RCLCPP_INFO(get_logger(),
      "CLOUD_PUB seq=%u points=%zu object=%p data=%p to_ros=%llu from_ros=%llu",
      sequence_, cloud->size(), static_cast<void *>(cloud.get()),
      static_cast<void *>(cloud->points.data()),
      static_cast<unsigned long long>(cloud_to_ros_count.load()),
      static_cast<unsigned long long>(cloud_from_ros_count.load()));
    publisher_->publish(std::move(cloud));
  }

  int64_t point_count_;
  uint32_t sequence_{0};
  rclcpp::Publisher<AdaptedCloud>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

class CloudSubscriber : public rclcpp::Node
{
public:
  explicit CloudSubscriber(const rclcpp::NodeOptions & options)
  : Node("cloud_subscriber", options)
  {
    subscription_ = create_subscription<AdaptedCloud>(
      "point_cloud", rclcpp::QoS(1).reliable(),
      [this](std::unique_ptr<PointCloud> cloud) {
        const auto sequence = cloud->empty() ? 0U : static_cast<uint32_t>(cloud->points[0].x);
        bool valid = !cloud->empty() && cloud->header.frame_id == "lab2_cloud" &&
          static_cast<uint64_t>(cloud->width) * cloud->height == cloud->size();
        for (std::size_t i = 0; i < cloud->size(); ++i) {
          const auto & point = cloud->points[i];
          valid = valid && point.x == static_cast<float>(sequence) &&
            point.y == static_cast<float>(i) &&
            point.z == static_cast<float>(sequence) + static_cast<float>(i) * 0.5F;
        }
        RCLCPP_INFO(get_logger(),
          "CLOUD_SUB seq=%u points=%zu object=%p data=%p valid=%s to_ros=%llu from_ros=%llu",
          sequence, cloud->size(), static_cast<void *>(cloud.get()),
          static_cast<void *>(cloud->points.data()), valid ? "true" : "false",
          static_cast<unsigned long long>(cloud_to_ros_count.load()),
          static_cast<unsigned long long>(cloud_from_ros_count.load()));
      });
    RCLCPP_INFO(get_logger(), "Cloud subscriber ready; intra_process=%s",
      options.use_intra_process_comms() ? "true" : "false");
  }

private:
  rclcpp::Subscription<AdaptedCloud>::SharedPtr subscription_;
};
}  // namespace lab2_namespace

RCLCPP_COMPONENTS_REGISTER_NODE(lab2_namespace::CloudPublisher)
RCLCPP_COMPONENTS_REGISTER_NODE(lab2_namespace::CloudSubscriber)
