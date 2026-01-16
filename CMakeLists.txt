cmake_minimum_required(VERSION 3.16)
project(lio_sam)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
add_definitions(-DBOOST_ALLOW_DEPRECATED_HEADERS)

add_compile_options(
    -Wno-reorder
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-sign-compare 
    -Wno-deprecated-declarations
    -Wno-comment
    -Wno-switch
    -Wno-maybe-uninitialized 
)

if(NOT MSVC)
    add_compile_options(-fexceptions -pthread)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|ARM|AARCH64)")
        add_compile_options(-O3 -mcpu=native -mtune=native -ffast-math)
        add_definitions(-DARM_ARCH)
    else()
        add_compile_options(-O3 -march=native -mtune=native -funroll-loops)
        add_definitions(-DX86_ARCH)
    endif()
endif()

find_package(Eigen3 REQUIRED)
find_package(PCL REQUIRED)
find_package(OpenCV REQUIRED)
find_package(GTSAM REQUIRED)
find_package(OpenMP REQUIRED)

find_package(rclcpp QUIET)

set(ROS_MSG_PKGS
    std_msgs
    sensor_msgs
    geometry_msgs
    nav_msgs
    visualization_msgs
    pcl_conversions
    tf2
    tf2_ros
    tf2_geometry_msgs 
    tf2_eigen
)

foreach(pkg ${ROS_MSG_PKGS})
    find_package(${pkg} REQUIRED)
endforeach()

include(ProcessorCount)
ProcessorCount(N)
if(N GREATER 1)
  add_definitions(-DMP_EN -DMP_PROC_NUM=${N})
endif()

add_library(${PROJECT_NAME} SHARED src/nodes.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/lio_sam>
    ${EIGEN3_INCLUDE_DIR}
    ${PCL_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${GTSAM_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    fins_sdk
    gtsam
    OpenMP::OpenMP_CXX
    ${PCL_LIBRARIES}
    ${OpenCV_LIBRARIES}
)

foreach(pkg ${ROS_MSG_PKGS})
    if(TARGET ${pkg}::${pkg}__rosidl_generator_cpp)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${pkg}::${pkg}__rosidl_generator_cpp)
    
    elseif(TARGET ${pkg}::${pkg})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${pkg}::${pkg})
    
    else()
        target_include_directories(${PROJECT_NAME} PRIVATE ${${pkg}_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${${pkg}_LIBRARIES})
    endif()
endforeach()

