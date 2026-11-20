#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/type_adapter.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/point_cloud2_iterator.hpp"

namespace lab2_namespace
{
using PointCloud = pcl::PointCloud<pcl::PointXYZ>;

// Спільні лічильники перетворень.
inline std::atomic<uint64_t> cloud_to_ros_count{0};
inline std::atomic<uint64_t> cloud_from_ros_count{0};

inline void log_cloud_conversions()
{
  RCLCPP_INFO(rclcpp::get_logger("point_cloud_adapter"), "ADAPTER to_ros=%llu from_ros=%llu",
    static_cast<unsigned long long>(cloud_to_ros_count.load()),
    static_cast<unsigned long long>(cloud_from_ros_count.load()));
}
}  // namespace lab2_namespace

template<>
struct rclcpp::TypeAdapter<lab2_namespace::PointCloud, sensor_msgs::msg::PointCloud2>
{
  using is_specialized = std::true_type;
  using custom_type = lab2_namespace::PointCloud;
  using ros_message_type = sensor_msgs::msg::PointCloud2;

  static void convert_to_ros_message(const custom_type & source, ros_message_type & destination)
  {
    if (static_cast<uint64_t>(source.width) * source.height != source.size()) {
      throw std::invalid_argument("PCL width and height do not match the point count");
    }
    destination = ros_message_type{};
    destination.header.frame_id = source.header.frame_id;
    destination.header.stamp.sec = static_cast<int32_t>(source.header.stamp / 1000000);
    destination.header.stamp.nanosec = static_cast<uint32_t>((source.header.stamp % 1000000) * 1000);
    sensor_msgs::PointCloud2Modifier modifier(destination);
    modifier.setPointCloud2Fields(3,
      "x", 1, sensor_msgs::msg::PointField::FLOAT32,
      "y", 1, sensor_msgs::msg::PointField::FLOAT32,
      "z", 1, sensor_msgs::msg::PointField::FLOAT32);
    modifier.resize(source.size());
    destination.width = source.width;
    destination.height = source.height;
    destination.row_step = destination.point_step * destination.width;
    destination.is_dense = source.is_dense;
    const uint16_t endian_probe = 1;
    destination.is_bigendian = *reinterpret_cast<const uint8_t *>(&endian_probe) == 0;
    if (!source.empty()) {
      sensor_msgs::PointCloud2Iterator<float> x(destination, "x");
      sensor_msgs::PointCloud2Iterator<float> y(destination, "y");
      sensor_msgs::PointCloud2Iterator<float> z(destination, "z");
      for (const auto & point : source.points) {
        *x = point.x;
        *y = point.y;
        *z = point.z;
        ++x; ++y; ++z;
      }
    }
    ++lab2_namespace::cloud_to_ros_count;
    lab2_namespace::log_cloud_conversions();
  }

  static void convert_to_custom(const ros_message_type & source, custom_type & destination)
  {
    const uint64_t count = static_cast<uint64_t>(source.width) * source.height;
    if (source.header.stamp.sec < 0 || count > 10000000 ||
      source.row_step < static_cast<uint64_t>(source.width) * source.point_step ||
      source.data.size() < static_cast<uint64_t>(source.height) * source.row_step)
    {
      throw std::invalid_argument("Invalid or excessive PointCloud2 dimensions/data size");
    }
    const auto offset = [&source](const char * name) {
        for (const auto & field : source.fields) {
          if (field.name == name && field.datatype == sensor_msgs::msg::PointField::FLOAT32 &&
            field.count == 1 && static_cast<uint64_t>(field.offset) + sizeof(float) <= source.point_step)
          {
            return field.offset;
          }
        }
        throw std::invalid_argument("PointCloud2 must contain FLOAT32 x, y and z fields");
      };
    const auto x = offset("x"), y = offset("y"), z = offset("z");
    const uint16_t endian_probe = 1;
    const bool host_bigendian = *reinterpret_cast<const uint8_t *>(&endian_probe) == 0;
    const auto read_float = [&](const uint8_t * address) {
        uint8_t bytes[sizeof(float)];
        std::memcpy(bytes, address, sizeof(float));
        if (source.is_bigendian != host_bigendian) {
          std::reverse(bytes, bytes + sizeof(float));
        }
        float value;
        std::memcpy(&value, bytes, sizeof(float));
        return value;
      };
    destination.resize(static_cast<std::size_t>(count));
    destination.width = source.width;
    destination.height = source.height;
    destination.is_dense = source.is_dense;
    destination.header.frame_id = source.header.frame_id;
    destination.header.stamp = static_cast<uint64_t>(source.header.stamp.sec) * 1000000 +
      source.header.stamp.nanosec / 1000;
    for (uint32_t row = 0; row < source.height; ++row) {
      for (uint32_t column = 0; column < source.width; ++column) {
        const auto * point = source.data.data() + static_cast<std::size_t>(row) * source.row_step +
          static_cast<std::size_t>(column) * source.point_step;
        auto & output = destination.points[static_cast<std::size_t>(row) * source.width + column];
        output.x = read_float(point + x);
        output.y = read_float(point + y);
        output.z = read_float(point + z);
      }
    }
    ++lab2_namespace::cloud_from_ros_count;
    lab2_namespace::log_cloud_conversions();
  }
};
