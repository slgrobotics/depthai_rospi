cmake_minimum_required(VERSION 3.16)
project(depthai_rospi VERSION 0.99.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
add_compile_options(-g)

## is used, also find other catkin packages
if(POLICY CMP0057)
    cmake_policy(SET CMP0057 NEW)
endif()

set(_opencv_version 4)
find_package(OpenCV 4 QUIET COMPONENTS imgproc highgui)
if(NOT OpenCV_FOUND)
  set(_opencv_version 3)
  find_package(OpenCV 3 REQUIRED COMPONENTS imgproc highgui)
endif()

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

find_package(ament_cmake REQUIRED)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "------------------------------------------")
message(STATUS "Depthai_rospi is being built using AMENT.")
message(STATUS "------------------------------------------")

find_package(camera_info_manager REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(depthai 3 CONFIG REQUIRED)
find_package(depthai_ros_msgs REQUIRED)
find_package(depthai_bridge REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(message_filters REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(image_transport REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(stereo_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)

# Link exported CMake targets directly; Lyrical removed ament_target_dependencies.
set(dependency_targets
  camera_info_manager::camera_info_manager
  cv_bridge::cv_bridge
  depthai::core
  depthai_bridge::depthai_bridge
  rclcpp::rclcpp
  rclcpp_components::component
  image_transport::image_transport
  message_filters::message_filters
  ${depthai_ros_msgs_TARGETS}
  ${geometry_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  ${stereo_msgs_TARGETS}
  ${std_msgs_TARGETS}
  ${vision_msgs_TARGETS}
  ${visualization_msgs_TARGETS}
  opencv_imgproc
  opencv_highgui
)

macro(dai_add_node_ros2 node_name node_src)
  add_executable("${node_name}" "${node_src}")

  target_include_directories("${node_name}" PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries("${node_name}" PRIVATE ${dependency_targets})

endmacro()


dai_add_node_ros2(stereo_node src/stereo_publisher.cpp)

add_library(
  spatial_bb_component SHARED
  src/spatial_bb.cpp
  src/utils.cpp
)

rclcpp_components_register_nodes(spatial_bb_component "depthai_rospi_filters::SpatialBB")

target_include_directories(spatial_bb_component PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(spatial_bb_component PRIVATE ${dependency_targets})

install(TARGETS spatial_bb_component
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY params DESTINATION share/${PROJECT_NAME})
install(DIRECTORY resources DESTINATION share/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

install(TARGETS
        stereo_node
        DESTINATION lib/${PROJECT_NAME})

ament_export_include_directories(
  include
)
install(
    DIRECTORY include/
    DESTINATION include
)

ament_package()

