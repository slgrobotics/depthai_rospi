cmake_minimum_required(VERSION 3.10.2)  # CMake version in Ubuntu 18.04 LTS
project(depthai_rospi VERSION 0.99.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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

# find_package(depthai CONFIG REQUIRED PATHS "/home/sachin/Desktop/luxonis/depthai-core/build/install/lib/cmake/depthai")
set(tiny_yolo_v4_blob_name "yolov4_tiny_coco_416x416_openvino_2021.4_6shave_bgr.blob")
set(mobilenet_blob_name "mobilenet-ssd_openvino_2021.2_6shave.blob")
set(deeplab_blob_name "deeplab_v3_plus_mnv2_decoder_256_openvino_2021.4.blob")

set(mobilenet_blob "${PROJECT_SOURCE_DIR}/resources/${mobilenet_blob_name}")
file(DOWNLOAD "https://artifacts.luxonis.com/artifactory/luxonis-depthai-data-local/network/mobilenet-ssd_openvino_2021.2_6shave.blob"
${mobilenet_blob}
TIMEOUT 60  # seconds
EXPECTED_HASH SHA1=f0e14978b3f77a4f93b9f969cd39e58bb7aef490
TLS_VERIFY ON)

set(tiny_yolo_v4_blob "${PROJECT_SOURCE_DIR}/resources/${tiny_yolo_v4_blob_name}")
file(DOWNLOAD "https://artifacts.luxonis.com/artifactory/luxonis-depthai-data-local/network/tiny-yolo-v4_openvino_2021.2_6shave.blob"
  ${tiny_yolo_v4_blob}
  TIMEOUT 60  # seconds
  EXPECTED_HASH SHA1=219d949610a5760e62a8458941e1300b81c3fe4a
  TLS_VERIFY ON)

set(deeplab_blob "${PROJECT_SOURCE_DIR}/resources/${deeplab_blob_name}")
file(DOWNLOAD
"https://github.com/luxonis/depthai-experiments/blob/master/gen2-deeplabv3_multiclass/models/deeplab_v3_plus_mnv2_decoder_256_openvino_2021.4.blob?raw=true"
  ${deeplab_blob}
  TIMEOUT 60  # seconds
  EXPECTED_HASH SHA1=bdc2529f765f34c4799b478690fb4e32f1ad5d89
  TLS_VERIFY ON)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_auto REQUIRED)
#ament_auto_find_build_dependencies()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "------------------------------------------")
message(STATUS "Depthai_rospi is being built using AMENT.")
message(STATUS "------------------------------------------")

set(BUILD_TOOL_INCLUDE_DIRS ${ament_INCLUDE_DIRS})

find_package(camera_info_manager REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(depthai CONFIG REQUIRED)
find_package(depthai_ros_msgs REQUIRED)
find_package(depthai_bridge REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(stereo_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(vision_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)

set(dependencies
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
  OpenCV
)

macro(dai_add_node_ros2 node_name node_src)
  add_executable("${node_name}" "${node_src}")

  target_link_libraries("${node_name}"
                        depthai::core
                        opencv_imgproc
                        opencv_highgui)

  ament_target_dependencies("${node_name}"
                            ${dependencies})

endmacro()


dai_add_node_ros2(stereo_node src/stereo_publisher.cpp)

include_directories(
  include
  ${ament_INCLUDE_DIRS}
)

ament_auto_add_library(
  spatial_bb_component SHARED
  src/spatial_bb.cpp
  src/utils.cpp
)

rclcpp_components_register_nodes(spatial_bb_component "depthai_rospi_filters::SpatialBB")

ament_target_dependencies(spatial_bb_component
  ${dependencies}
)

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

