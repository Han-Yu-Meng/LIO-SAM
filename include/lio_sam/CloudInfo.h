#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <sensor_msgs/msg/point_cloud2.hpp>
#include <std_msgs/msg/header.hpp>

namespace lio_sam {
namespace msg {

struct CloudInfo {
  std_msgs::msg::Header header;

  std::vector<int32_t> start_ring_index;
  std::vector<int32_t> end_ring_index;

  std::vector<int32_t> point_col_ind;
  std::vector<float> point_range;

  int64_t imu_available;
  int64_t odom_available;

  float imu_roll_init;
  float imu_pitch_init;
  float imu_yaw_init;

  float initial_guess_x;
  float initial_guess_y;
  float initial_guess_z;
  float initial_guess_roll;
  float initial_guess_pitch;
  float initial_guess_yaw;

  sensor_msgs::msg::PointCloud2 cloud_deskewed;
  sensor_msgs::msg::PointCloud2 cloud_corner;
  sensor_msgs::msg::PointCloud2 cloud_surface;

  using SharedPtr = std::shared_ptr<CloudInfo>;
  using ConstSharedPtr = std::shared_ptr<const CloudInfo>;
};

} // namespace msg
} // namespace lio_sam
