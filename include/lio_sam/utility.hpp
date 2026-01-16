#pragma once
#ifndef _UTILITY_LIDAR_ODOMETRY_H_
#define _UTILITY_LIDAR_ODOMETRY_H_

#include <iostream>

#include <std_msgs/msg/header.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/path.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <CloudInfo.h>

#include <opencv2/opencv.hpp>

#include <pcl/kdtree/kdtree_flann.h>  // pcl include kdtree_flann throws error if PCL_NO_PRECOMPILE
                                      // is defined before
#define PCL_NO_PRECOMPILE
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/search/impl/search.hpp>
#include <pcl/range_image/range_image.h>
#include <pcl/common/common.h>
#include <pcl/common/transforms.h>
#include <pcl/registration/icp.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/filter.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/crop_box.h>
#include <pcl_conversions/pcl_conversions.h>

#include <tf2/LinearMath/Quaternion.h>
#include <tf2_eigen/tf2_eigen.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <vector>
#include <cmath>
#include <algorithm>
#include <queue>
#include <deque>
#include <iostream>
#include <fstream>
#include <ctime>
#include <cfloat>
#include <iterator>
#include <sstream>
#include <string>
#include <limits>
#include <iomanip>
#include <array>
#include <thread>
#include <mutex>

#include <fins/node.hpp>
#include <fins/agent/parameter_server.hpp>

using namespace std;

typedef pcl::PointXYZI PointType;

enum class SensorType { VELODYNE, OUSTER, LIVOX };

class ParamServer : public fins::Node
{
public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    
    std::string robot_id;

    //Frames
    string lidarFrame;
    string baselinkFrame;
    string odometryFrame;
    string mapFrame;

    // GPS Settings
    bool useImuHeadingInitialization;
    bool useGpsElevation;
    float gpsCovThreshold;
    float poseCovThreshold;

    // Save pcd
    bool savePCD;
    string savePCDDirectory;

    // Lidar Sensor Configuration
    SensorType sensor = SensorType::OUSTER;
    int N_SCAN;
    int Horizon_SCAN;
    int downsampleRate;
    float lidarMinRange;
    float lidarMaxRange;

    // IMU
    float imuAccNoise;
    float imuGyrNoise;
    float imuAccBiasN;
    float imuGyrBiasN;
    float imuGravity;
    float imuRPYWeight;
    vector<double> extRotV;
    vector<double> extRPYV;
    vector<double> extTransV;
    Eigen::Matrix3d extRot;
    Eigen::Matrix3d extRPY;
    Eigen::Vector3d extTrans;
    Eigen::Quaterniond extQRPY;

    // LOAM
    float edgeThreshold;
    float surfThreshold;
    int edgeFeatureMinValidNum;
    int surfFeatureMinValidNum;

    // voxel filter paprams
    float odometrySurfLeafSize;
    float mappingCornerLeafSize;
    float mappingSurfLeafSize ;

    float z_tollerance;
    float rotation_tollerance;

    // CPU Params
    int numberOfCores;
    double mappingProcessInterval;

    // Surrounding map
    float surroundingkeyframeAddingDistThreshold;
    float surroundingkeyframeAddingAngleThreshold;
    float surroundingKeyframeDensity;
    float surroundingKeyframeSearchRadius;

    // Loop closure
    bool  loopClosureEnableFlag;
    float loopClosureFrequency;
    int   surroundingKeyframeSize;
    float historyKeyframeSearchRadius;
    float historyKeyframeSearchTimeDiff;
    int   historyKeyframeSearchNum;
    float historyKeyframeFitnessScore;

    // global map visualization radius
    float globalMapVisualizationSearchRadius;
    float globalMapVisualizationPoseDensity;
    float globalMapVisualizationLeafSize;


    virtual void initialize() override
    {
        fins::ParamLoader param("LIO_SAM");

        lidarFrame = param.get("lidarFrame", "laser_data_frame");
        baselinkFrame = param.get("baselinkFrame", "base_link");
        odometryFrame = param.get("odometryFrame", "odom");
        mapFrame = param.get("mapFrame", "map");

        useImuHeadingInitialization = param.get("useImuHeadingInitialization", false);
        useGpsElevation = param.get("useGpsElevation", false);
        gpsCovThreshold = param.get("gpsCovThreshold", 2.0);
        poseCovThreshold = param.get("poseCovThreshold", 25.0);

        std::string sensorStr = param.get("sensor", "ouster");

        if (sensorStr == "velodyne") {
            sensor = SensorType::VELODYNE;
        }
        else if (sensorStr == "ouster") {
            sensor = SensorType::OUSTER;
        }
        else if (sensorStr == "livox") {
            sensor = SensorType::LIVOX;
        }
        else {
            logger->error("Unknown sensor type: {}", sensorStr);
        }

        N_SCAN = param.get("N_SCAN", 64);
        Horizon_SCAN = param.get("Horizon_SCAN", 512);
        downsampleRate = param.get("downsampleRate", 1);
        lidarMinRange = param.get("lidarMinRange", 5.5);
        lidarMaxRange = param.get("lidarMaxRange", 1000.0);

        imuAccNoise = param.get("imuAccNoise", 9e-4);
        imuGyrNoise = param.get("imuGyrNoise", 1.6e-4);
        imuAccBiasN = param.get("imuAccBiasN", 5e-4);
        imuGyrBiasN = param.get("imuGyrBiasN", 7e-5);
        imuGravity = param.get("imuGravity", 9.80511);
        imuRPYWeight = param.get("imuRPYWeight", 0.01);

        extRotV = param.get("extrinsicRot", std::vector<double>{1.0, 0.0, 0.0,
                                                            0.0, 1.0, 0.0,
                                                            0.0, 0.0, 1.0});

        extRPYV = param.get("extrinsicRPY", std::vector<double>{1.0, 0.0, 0.0,
                                                            0.0, 1.0, 0.0,
                                                            0.0, 0.0, 1.0});

        extTransV = param.get("extrinsicTrans", std::vector<double>{0.0, 0.0, 0.0});

        extRot = Eigen::Map<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>>(extRotV.data(), 3, 3);
        extRPY = Eigen::Map<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>>(extRPYV.data(), 3, 3);
        extTrans = Eigen::Map<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>>(extTransV.data(), 3, 1);
        extQRPY = Eigen::Quaterniond(extRPY);

        edgeThreshold = param.get("edgeThreshold", 1.0);
        surfThreshold = param.get("surfThreshold", 0.1);
        edgeFeatureMinValidNum = param.get("edgeFeatureMinValidNum", 10);
        surfFeatureMinValidNum = param.get("surfFeatureMinValidNum", 100);

        odometrySurfLeafSize = param.get("odometrySurfLeafSize", 0.4);
        mappingCornerLeafSize = param.get("mappingCornerLeafSize", 0.2);
        mappingSurfLeafSize = param.get("mappingSurfLeafSize", 0.4);

        z_tollerance = param.get("z_tollerance", 1000.0);
        rotation_tollerance = param.get("rotation_tollerance", 1000.0);

        numberOfCores = param.get("numberOfCores", 4);
        mappingProcessInterval = param.get("mappingProcessInterval", 0.15);

        surroundingkeyframeAddingDistThreshold = param.get("surroundingkeyframeAddingDistThreshold", 1.0);
        surroundingkeyframeAddingAngleThreshold = param.get("surroundingkeyframeAddingAngleThreshold", 0.2);
        surroundingKeyframeDensity = param.get("surroundingKeyframeDensity", 2.0);
        surroundingKeyframeSearchRadius = param.get("surroundingKeyframeSearchRadius", 50.0);

        loopClosureEnableFlag = param.get("loopClosureEnableFlag", true);
        loopClosureFrequency = param.get("loopClosureFrequency", 1.0);
        surroundingKeyframeSize = param.get("surroundingKeyframeSize", 50);
        historyKeyframeSearchRadius = param.get("historyKeyframeSearchRadius", 15.0);
        historyKeyframeSearchTimeDiff = param.get("historyKeyframeSearchTimeDiff", 30.0);
        historyKeyframeSearchNum = param.get("historyKeyframeSearchNum", 25);
        historyKeyframeFitnessScore = param.get("historyKeyframeFitnessScore", 0.3);

        globalMapVisualizationSearchRadius = param.get("globalMapVisualizationSearchRadius", 1000.0);
        globalMapVisualizationPoseDensity = param.get("globalMapVisualizationPoseDensity", 10.0);
        globalMapVisualizationLeafSize = param.get("globalMapVisualizationLeafSize", 1.0);
    }

    virtual void define() {

    }

    sensor_msgs::msg::Imu imuConverter(const sensor_msgs::msg::Imu& imu_in)
    {
        sensor_msgs::msg::Imu imu_out = imu_in;
        // rotate acceleration
        Eigen::Vector3d acc(imu_in.linear_acceleration.x, imu_in.linear_acceleration.y, imu_in.linear_acceleration.z);
        acc = extRot * acc;
        imu_out.linear_acceleration.x = acc.x();
        imu_out.linear_acceleration.y = acc.y();
        imu_out.linear_acceleration.z = acc.z();
        // rotate gyroscope
        Eigen::Vector3d gyr(imu_in.angular_velocity.x, imu_in.angular_velocity.y, imu_in.angular_velocity.z);
        gyr = extRot * gyr;
        imu_out.angular_velocity.x = gyr.x();
        imu_out.angular_velocity.y = gyr.y();
        imu_out.angular_velocity.z = gyr.z();
        // rotate roll pitch yaw
        Eigen::Quaterniond q_from(imu_in.orientation.w, imu_in.orientation.x, imu_in.orientation.y, imu_in.orientation.z);
        Eigen::Quaterniond q_final = q_from * extQRPY;
        imu_out.orientation.x = q_final.x();
        imu_out.orientation.y = q_final.y();
        imu_out.orientation.z = q_final.z();
        imu_out.orientation.w = q_final.w();

        if (sqrt(q_final.x()*q_final.x() + q_final.y()*q_final.y() + q_final.z()*q_final.z() + q_final.w()*q_final.w()) < 0.1)
        {
            logger->error("Invalid quaternion, please use a 9-axis IMU!");
        }

        return imu_out;
    }
};

sensor_msgs::msg::PointCloud2 pcl_to_ros(pcl::PointCloud<PointType>::Ptr thisCloud, rclcpp::Time thisStamp, std::string thisFrame)
{
    sensor_msgs::msg::PointCloud2 tempCloud;
    pcl::toROSMsg(*thisCloud, tempCloud);
    tempCloud.header.stamp = thisStamp;
    tempCloud.header.frame_id = thisFrame;
    return tempCloud;
}

template<typename T>
double stamp2Sec(const T& stamp)
{
    return rclcpp::Time(stamp).seconds();
}


template<typename T>
void imuAngular2rosAngular(sensor_msgs::msg::Imu *thisImuMsg, T *angular_x, T *angular_y, T *angular_z)
{
    *angular_x = thisImuMsg->angular_velocity.x;
    *angular_y = thisImuMsg->angular_velocity.y;
    *angular_z = thisImuMsg->angular_velocity.z;
}


template<typename T>
void imuAccel2rosAccel(sensor_msgs::msg::Imu *thisImuMsg, T *acc_x, T *acc_y, T *acc_z)
{
    *acc_x = thisImuMsg->linear_acceleration.x;
    *acc_y = thisImuMsg->linear_acceleration.y;
    *acc_z = thisImuMsg->linear_acceleration.z;
}


template<typename T>
void imuRPY2rosRPY(sensor_msgs::msg::Imu *thisImuMsg, T *rosRoll, T *rosPitch, T *rosYaw)
{
    double imuRoll, imuPitch, imuYaw;
    tf2::Quaternion orientation;
    tf2::fromMsg(thisImuMsg->orientation, orientation);
    tf2::Matrix3x3(orientation).getRPY(imuRoll, imuPitch, imuYaw);

    *rosRoll = imuRoll;
    *rosPitch = imuPitch;
    *rosYaw = imuYaw;
}


float pointDistance(PointType p)
{
    return sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
}


float pointDistance(PointType p1, PointType p2)
{
    return sqrt((p1.x-p2.x)*(p1.x-p2.x) + (p1.y-p2.y)*(p1.y-p2.y) + (p1.z-p2.z)*(p1.z-p2.z));
}

#endif
