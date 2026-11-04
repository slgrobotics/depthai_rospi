#include <cstdio>
#include <functional>
#include <iostream>
#include <tuple>
#include <sys/syscall.h> // for SYS_gettid
#include <unistd.h>

#include "camera_info_manager/camera_info_manager.hpp"
#include "rclcpp/executors.hpp"
#include "rclcpp/node.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "stereo_msgs/msg/disparity_image.hpp"

// Inludes common necessary includes for development using depthai library
#include "depthai/pipeline/MessageQueue.hpp"
#include "depthai/device/Device.hpp"
#include "depthai/pipeline/Pipeline.hpp"
#include "depthai/pipeline/node/MonoCamera.hpp"
#include "depthai/pipeline/node/StereoDepth.hpp"

#include "BridgePublisherPi.hpp"
#include "depthai_bridge/DisparityConverter.hpp"
#include "depthai_bridge/ImageConverter.hpp"

//#define TRACE

std::tuple<dai::Pipeline, int, int, std::shared_ptr<dai::MessageQueue>,
           std::shared_ptr<dai::MessageQueue>, std::shared_ptr<dai::MessageQueue>> createPipeline(
                bool lrcheck, bool extended, bool subpixel,
                int confidence, int LRchecktresh, std::string resolution)
{
#ifdef TRACE
    std::cout << "IP: createPipeline()" << std::endl;
    std::cout << "FYI: lrcheck: " << lrcheck << "  extended: " << extended << "  subpixel: " << subpixel << std::endl;
    std::cout << "FYI: confidence: " << confidence << "  LRchecktresh: " << LRchecktresh << "  resolution: " << resolution << std::endl;
#endif // TRACE

    dai::Pipeline pipeline;
    dai::node::MonoCamera::Properties::SensorResolution monoResolution;
    auto monoLeft = pipeline.create<dai::node::MonoCamera>();
    auto monoRight = pipeline.create<dai::node::MonoCamera>();
    auto stereo = pipeline.create<dai::node::StereoDepth>();

    // XLinkOut


    int width, height;
    if(resolution == "720p") {
        monoResolution = dai::node::MonoCamera::Properties::SensorResolution::THE_720_P;
        width = 1280;
        height = 720;
    } else if(resolution == "400p") {
        monoResolution = dai::node::MonoCamera::Properties::SensorResolution::THE_400_P;
        width = 640;
        height = 400;
    } else if(resolution == "800p") {
        monoResolution = dai::node::MonoCamera::Properties::SensorResolution::THE_800_P;
        width = 1280;
        height = 800;
    } else if(resolution == "480p") {
        monoResolution = dai::node::MonoCamera::Properties::SensorResolution::THE_480_P;
        width = 640;
        height = 480;
    } else {
        RCLCPP_ERROR(rclcpp::get_logger("rclcpp"), "Invalid parameter. -> monoResolution: %s", resolution.c_str());
        throw std::runtime_error("Invalid mono camera resolution.");
    }

    // MonoCamera
    monoLeft->setResolution(monoResolution);
    monoLeft->setBoardSocket(dai::CameraBoardSocket::CAM_B);
    monoRight->setResolution(monoResolution);
    monoRight->setBoardSocket(dai::CameraBoardSocket::CAM_C);

    // StereoDepth
    stereo->initialConfig->setConfidenceThreshold(confidence);
    stereo->setRectifyEdgeFillColor(0);  // black, to better see the cutout
    stereo->initialConfig->setLeftRightCheckThreshold(LRchecktresh);
    stereo->setLeftRightCheck(lrcheck);
    stereo->setExtendedDisparity(extended);
    stereo->setSubpixel(subpixel);

    // Link plugins CAM -> STEREO -> XLINK
    monoLeft->out.link(stereo->left);
    monoRight->out.link(stereo->right);

    auto leftQueue = stereo->rectifiedLeft.createOutputQueue(30, false);
    auto rightQueue = stereo->rectifiedRight.createOutputQueue(30, false);
    auto depthQueue = stereo->depth.createOutputQueue(30, false);

    return std::make_tuple(pipeline, width, height, leftQueue, rightQueue, depthQueue);
}

static rclcpp::Node::SharedPtr node;

static dai::rosBridge::BridgePublisherPi<sensor_msgs::msg::Image, dai::ImgFrame>* leftPublish_ = nullptr;
static dai::rosBridge::BridgePublisherPi<sensor_msgs::msg::Image, dai::ImgFrame>* rightPublish_ = nullptr;
static dai::rosBridge::BridgePublisherPi<sensor_msgs::msg::Image, dai::ImgFrame>* depthPublish_ = nullptr;

static std::string tfPrefix, monoResolution_;
static bool lrcheck_, extended_, subpixel_;
static int confidence_, LRchecktresh_;
static int monoWidth, monoHeight;
static int fpsDivider_ = 1;

static int cnt_freezes_ = 0;

void runPublishers()
{
    dai::Pipeline pipeline(false);

#ifdef TRACE
    std::cout << "IP: runPublishers()" << std::endl;
    std::cout << "FYI: tfPrefix: " << tfPrefix << "  monoResolution_: " << monoResolution_ << std::endl;
#endif // TRACE

    std::shared_ptr<dai::MessageQueue> leftQueue, rightQueue, stereoQueue;
    std::tie(pipeline, monoWidth, monoHeight, leftQueue, rightQueue, stereoQueue) =
        createPipeline(lrcheck_, extended_, subpixel_, confidence_, LRchecktresh_, monoResolution_);
    auto device = pipeline.getDefaultDevice();
    pipeline.start();

    auto calibrationHandler = device->readCalibration();

    auto boardName = calibrationHandler.getEepromData().boardName;
#ifdef TRACE
    std::cout << "FYI: boardName: " << boardName << std::endl;
#endif // TRACE
    if(monoHeight > 480 && boardName == "OAK-D-LITE") {
        monoWidth = 640;
        monoHeight = 480;
    }

    depthai_bridge::ImageConverter converter(tfPrefix + "_left_camera_optical_frame", true);
    auto leftCameraInfo = converter.calibrationToCameraInfo(calibrationHandler, dai::CameraBoardSocket::CAM_B, monoWidth, monoHeight);
    dai::rosBridge::BridgePublisherPi<sensor_msgs::msg::Image, dai::ImgFrame> leftPublish(
        leftQueue,
        node,
        std::string("left/image_rect"),
        [&converter](std::shared_ptr<dai::ImgFrame> frame, std::deque<sensor_msgs::msg::Image>& messages) {
            converter.toRosMsg(frame, messages);
        },
        30,
        leftCameraInfo,
        "left");

    //leftPublish.addPublisherCallback();
    leftPublish_ = &leftPublish;
    leftPublish.setFpsDivider(fpsDivider_);   
    leftPublish.startPublisherThread();

    depthai_bridge::ImageConverter rightconverter(tfPrefix + "_right_camera_optical_frame", true);
    auto rightCameraInfo = converter.calibrationToCameraInfo(calibrationHandler, dai::CameraBoardSocket::CAM_C, monoWidth, monoHeight);
    dai::rosBridge::BridgePublisherPi<sensor_msgs::msg::Image, dai::ImgFrame> rightPublish(
        rightQueue,
        node,
        std::string("right/image_rect"),
        [&rightconverter](std::shared_ptr<dai::ImgFrame> frame, std::deque<sensor_msgs::msg::Image>& messages) {
            rightconverter.toRosMsg(frame, messages);
        },
        30,
        rightCameraInfo,
        "right");

    //rightPublish.addPublisherCallback();
    rightPublish_ = &rightPublish;
    rightPublish.setFpsDivider(fpsDivider_);   
    rightPublish.startPublisherThread();

    depthai_bridge::ImageConverter rightconverter_(tfPrefix + "_right_camera_optical_frame", true);
    auto rightCameraInfo_ = converter.calibrationToCameraInfo(calibrationHandler, dai::CameraBoardSocket::CAM_C, monoWidth, monoHeight);
    dai::rosBridge::BridgePublisherPi<sensor_msgs::msg::Image, dai::ImgFrame> depthPublish(
        stereoQueue,
        node,
        std::string("stereo/depth"),
        [&rightconverter_](std::shared_ptr<dai::ImgFrame> frame, std::deque<sensor_msgs::msg::Image>& messages) {
            rightconverter_.toRosMsg(frame, messages);
        },
        30,
        rightCameraInfo_,
        "stereo");
    //depthPublish.addPublisherCallback();
    depthPublish_ = &depthPublish;
    depthPublish.setFpsDivider(fpsDivider_);   
    depthPublish.startPublisherThread();

    // Keep the pipeline, converters, and publishers alive while frames are consumed.
    while(rclcpp::ok() && pipeline.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    pipeline.stop();
    leftPublish.stopPublisherThread();
    rightPublish.stopPublisherThread();
    depthPublish.stopPublisherThread();
}

void workerTask()
{
    if(rclcpp::ok()) {
        pid_t id = syscall(SYS_gettid);
#ifdef TRACE
        std::cout << "IP: workerTask() - in thread ID: " << id << std::endl;
        std::cout << "IP: Initializing new publishers..." << std::endl;
#endif // TRACE

        try {
            runPublishers(); // will block until it crashes
            std::cout << "FYI: Publishers exited" << std::endl;
        } catch(...)
        {
            std::cerr << "workerTask: Caught an exception in runPublishers()" << std::endl;
        }
    }
}

void monitoringTask()
{
    pid_t id = syscall(SYS_gettid);
#ifdef TRACE
    std::cout << "IP: monitoringTask() - in thread ID: " << id << std::endl;
#endif // TRACE

    while(rclcpp::ok()) {

        unsigned int last_received = 0;

#ifdef TRACE
        std::cout << "IP: Outer loop of monitoring thread " << id << std::endl;
        std::cout << "IP: starting worker thread..." << std::endl;
#endif // TRACE

        std::thread workerThread;

        try {
            workerThread = std::thread(workerTask);

            while(leftPublish_ == nullptr || rightPublish_ == nullptr || depthPublish_ == nullptr) {
#ifdef TRACE
                std::cout << "IP: waiting for Publishers to be initialized..." << std::endl;
#endif // TRACE
                sleep(1);
            }

            std::cout << "OK: Publishers initialized" << std::endl;

            while (rclcpp::ok()) {

                sleep(1);

                unsigned int received = depthPublish_->getReceivedCount();

#ifdef TRACE
                // \x1b[1;A  // (CSI 1 A) move cursor up a line.
                // \r     // (CR) move cursor to beginning of line.
                // \x1b[0;K  // (CSI 0 K) clear line from cursor to EOL.

                std::cout << "\x1b[1;A\rIP: Inner loop of monitoring thread, received: " << received << std::endl;
#endif // TRACE

                if (received == last_received) {
                    cnt_freezes_++;
                    std::cerr << "Error: frozen OAK device pipeline,  cnt_freezes: " << cnt_freezes_ << std::endl;
                    break;
                }

                last_received = received;
            }
        } catch (...) {
            std::cerr << "monitoringTask: Caught an exception while starting or running the worker thread" << std::endl;
        }

        if (workerThread.joinable()) {
            workerThread.join();
        }
        leftPublish_ = rightPublish_ = depthPublish_ = nullptr;
    }
}

int main(int argc, char** argv)
{

#ifdef TRACE
    std::cout << "IP: main()" << std::endl;
#endif // TRACE

    rclcpp::init(argc, argv);
    node = rclcpp::Node::make_shared("stereo_node");

    node->declare_parameter("tf_prefix", "oak");
    node->declare_parameter("lrcheck", true);
    node->declare_parameter("extended", false);
    node->declare_parameter("subpixel", true);
    node->declare_parameter("confidence", 200);
    node->declare_parameter("LRchecktresh", 5);
    node->declare_parameter("monoResolution", "720p");
    node->declare_parameter("fpsDivider", 10);

    node->get_parameter("tf_prefix", tfPrefix);
    node->get_parameter("lrcheck", lrcheck_);
    node->get_parameter("extended", extended_);
    node->get_parameter("subpixel", subpixel_);
    node->get_parameter("confidence", confidence_);
    node->get_parameter("LRchecktresh", LRchecktresh_);
    node->get_parameter("monoResolution", monoResolution_);
    node->get_parameter("fpsDivider", fpsDivider_);

    std::thread monitoringThread;

    try {
#ifdef TRACE
        std::cout << "IP: starting monitoring thread..." << std::endl;
#endif // TRACE

        monitoringThread = std::thread(monitoringTask);

#ifdef TRACE
        std::cout << "IP: spinning node..." << std::endl;
#endif // TRACE

        rclcpp::spin(node);

    } catch (...) {
        std::cerr << "monitoringTask: Caught an exception while starting or running the worker thread" << std::endl;
    }

#ifdef TRACE
    std::cout << "IP: main(): shutting down all threads " << std::endl;
#endif // TRACE

    if (monitoringThread.joinable()) {
        monitoringThread.join();
    }
    if (leftPublish_ != nullptr) {
        leftPublish_->stopPublisherThread();
    }
    if (rightPublish_ != nullptr) {
        rightPublish_->stopPublisherThread();
    }
    if (depthPublish_ != nullptr) {
        depthPublish_->stopPublisherThread();
    }
    rclcpp::shutdown();

#ifdef TRACE
    std::cout << "OK: main() finished" << std::endl;
#endif // TRACE

    return 0;
}

