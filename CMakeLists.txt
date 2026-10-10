cmake_minimum_required(VERSION 3.16)
project(depthai_rospi VERSION 0.99.1 LANGUAGES CXX C)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------

find_package(ament_cmake REQUIRED)

find_package(OpenCV 4 REQUIRED COMPONENTS imgproc highgui)

find_package(depthai CONFIG REQUIRED)
find_package(depthai_bridge REQUIRED)
find_package(depthai_ros_msgs REQUIRED)

find_package(camera_info_manager REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(message_filters REQUIRED)

find_package(sensor_msgs REQUIRED)
find_package(stereo_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)

message(STATUS "------------------------------------------")
message(STATUS "Building depthai_rospi using ROS 2 AMENT")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "------------------------------------------")

# ------------------------------------------------------------
# Neural network resources
# ------------------------------------------------------------

set(tiny_yolo_v4_blob_name
  "yolov4_tiny_coco_416x416_openvino_2021.4_6shave_bgr.blob"
)

set(mobilenet_blob_name
  "mobilenet-ssd_openvino_2021.2_6shave.blob"
)

set(deeplab_blob_name
  "deeplab_v3_plus_mnv2_decoder_256_openvino_2021.4.blob"
)

set(RESOURCE_DIR "${PROJECT_SOURCE_DIR}/resources")
file(MAKE_DIRECTORY "${RESOURCE_DIR}")

function(download_blob filename url sha1)
  set(destination "${RESOURCE_DIR}/${filename}")

  set(download_required TRUE)

  if(EXISTS "${destination}")
    file(SHA1 "${destination}" existing_hash)

    if(existing_hash STREQUAL sha1)
      set(download_required FALSE)
      message(STATUS "Using existing blob: ${filename}")
    endif()
  endif()

  if(download_required)
    message(STATUS "Downloading blob: ${filename}")

    file(DOWNLOAD
      "${url}"
      "${destination}"
      TIMEOUT 60
      EXPECTED_HASH "SHA1=${sha1}"
      TLS_VERIFY ON
      STATUS download_status
    )

    list(GET download_status 0 status_code)
    list(GET download_status 1 status_message)

    if(NOT status_code EQUAL 0)
      message(FATAL_ERROR
        "Failed to download ${filename}: ${status_message}"
      )
    endif()
  endif()
endfunction()

download_blob(
  "${mobilenet_blob_name}"
  "https://artifacts.luxonis.com/artifactory/luxonis-depthai-data-local/network/mobilenet-ssd_openvino_2021.2_6shave.blob"
  "f0e14978b3f77a4f93b9f969cd39e58bb7aef490"
)

download_blob(
  "${tiny_yolo_v4_blob_name}"
  "https://artifacts.luxonis.com/artifactory/luxonis-depthai-data-local/network/tiny-yolo-v4_openvino_2021.2_6shave.blob"
  "219d949610a5760e62a8458941e1300b81c3fe4a"
)

download_blob(
  "${deeplab_blob_name}"
  "https://github.com/luxonis/depthai-experiments/blob/master/gen2-deeplabv3_multiclass/models/deeplab_v3_plus_mnv2_decoder_256_openvino_2021.4.blob?raw=true"
  "bdc2529f765f34c4799b478690fb4e32f1ad5d89"
)

# ------------------------------------------------------------
# Dependency linking helper
# ------------------------------------------------------------

# Prefer modern CMake targets.
# Fall back to legacy ament exports where necessary.

function(link_ros_dependency target dependency)

  if(TARGET "${dependency}::${dependency}")

    target_link_libraries(${target} PRIVATE
      "${dependency}::${dependency}"
    )

  elseif(DEFINED ${dependency}_TARGETS
         AND NOT "${${dependency}_TARGETS}" STREQUAL "")

    target_link_libraries(${target} PRIVATE
      ${${dependency}_TARGETS}
    )

  else()

    # Legacy packages may export include directories and libraries
    # without providing imported CMake targets.

    if(DEFINED ${dependency}_INCLUDE_DIRS)
      target_include_directories(${target} PRIVATE
        ${${dependency}_INCLUDE_DIRS}
      )
    endif()

    if(DEFINED ${dependency}_LIBRARIES)
      target_link_libraries(${target} PRIVATE
        ${${dependency}_LIBRARIES}
      )
    endif()

  endif()

endfunction()

set(ros_dependencies
  camera_info_manager
  cv_bridge
  depthai_ros_msgs
  depthai_bridge
  rclcpp
  rclcpp_components
  sensor_msgs
  stereo_msgs
  std_msgs
  vision_msgs
  visualization_msgs
  message_filters
)

function(configure_ros_target target)

  target_include_directories(${target} PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
  )

  target_link_libraries(${target} PRIVATE
    depthai::core
    ${OpenCV_LIBS}
  )

  target_include_directories(${target} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
  )

  foreach(dependency IN LISTS ros_dependencies)
    link_ros_dependency(${target} ${dependency})
  endforeach()

endfunction()

# ------------------------------------------------------------
# Stereo publisher executable
# ------------------------------------------------------------

add_executable(stereo_node
  src/stereo_publisher.cpp
)

configure_ros_target(stereo_node)

# ------------------------------------------------------------
# Spatial bounding box component
# ------------------------------------------------------------

add_library(spatial_bb_component SHARED
  src/spatial_bb.cpp
  src/utils.cpp
)

configure_ros_target(spatial_bb_component)

rclcpp_components_register_nodes(
  spatial_bb_component
  "depthai_rospi_filters::SpatialBB"
)

# ------------------------------------------------------------
# Installation
# ------------------------------------------------------------

install(
  TARGETS spatial_bb_component
  EXPORT export_spatial_bb_component
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  TARGETS stereo_node
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY
    params
    resources
    launch
    config
  DESTINATION share/${PROJECT_NAME}
)

install(
  DIRECTORY include/
  DESTINATION include
)

# ------------------------------------------------------------
# Exports
# ------------------------------------------------------------

ament_export_targets(
  export_spatial_bb_component
  HAS_LIBRARY_TARGET
)

ament_export_dependencies(
  camera_info_manager
  cv_bridge
  depthai_ros_msgs
  depthai_bridge
  rclcpp
  rclcpp_components
  sensor_msgs
  stereo_msgs
  std_msgs
  vision_msgs
  visualization_msgs
  message_filters
)

ament_package()
