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

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer")
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(GTSAM_BUILD_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(GTSAM_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)

set(GTSAM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GTSAM_BUILD_EXAMPLES_ALWAYS OFF CACHE BOOL "" FORCE)
set(GTSAM_BUILD_TIMING_ALWAYS OFF CACHE BOOL "" FORCE)
set(GTSAM_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GTSAM_INSTALL_HEADERS ON CACHE BOOL "" FORCE)

set(GTSAM_USE_SYSTEM_EIGEN ON CACHE BOOL "" FORCE)

set(GTSAM_WITH_TBB ON CACHE BOOL "" FORCE) 

set(GTSAM_BUILD_WITH_MARCH_NATIVE ON CACHE BOOL "" FORCE)

add_subdirectory(external/gtsam)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/gtsam/gtsam)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/external/gtsam)

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

include(ProcessorCount)
ProcessorCount(N)
if(N GREATER 1)
  add_definitions(-DMP_EN -DMP_PROC_NUM=${N})
endif()

fins_add_node(${PROJECT_NAME} src/nodes.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/lio_sam>
    ${EIGEN3_INCLUDE_DIR}
    ${PCL_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    gtsam 
    OpenMP::OpenMP_CXX
    ${PCL_LIBRARIES}
    ${OpenCV_LIBRARIES}
)

add_dependencies(${PROJECT_NAME} gtsam)

fins_link_ros_dependencies(${PROJECT_NAME}
    rclcpp 
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

