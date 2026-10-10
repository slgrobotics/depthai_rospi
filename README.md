
## OAK-D Lite on a Raspberry Pi 5 – *depthai_rospi* package

### Important: DepthAI 2.x / 3.x incompatibility

OAK-D cameras support for ROS2 **Lyrical Luth** is installed by:
```
sudo apt install ros-${ROS_DISTRO}-depthai-ros
    Installing:                     
      ros-lyrical-depthai-ros
    Installing dependencies:
      ros-lyrical-depthai-examples  ros-lyrical-depthai-filters
```

The old **Jazzy** DepthAI 2.x version is now replaced by:
- DepthAI [version: 3.10.0](https://docs.luxonis.com/software-v3/depthai/)
- DepthAI CMake directory: /opt/ros/lyrical/lib/aarch64-linux-gnu/cmake/depthai
- `depthai_bridge`: 3.4.0

This branch is work in progress, not finished yet.

---------------------------------------

**Note:** Review this [guide](https://github.com/slgrobotics/robots_bringup/blob/main/Docs/Sensors/OAK-D_Lite.md) first.

This is a ROS 2 (Lyrical Luth) package designed to work with the _OAK-D Lite_ camera on a Raspberry Pi 5.  
It is based on the [depthai_examples](https://github.com/luxonis/depthai-ros/tree/humble/depthai_examples) package.

Luxonis cameras deliver data through _pipelines_, and when running at high frame rates (e.g., 30 FPS), they can overwhelm the ROS network, with data rates exceeding 700 Mbit/s.

There is a known issue: the _OAK-D Lite_ does not support changing FPS via the `i_fps` parameter or programmatically in code:  
https://github.com/luxonis/depthai/issues/624

In my experiments with the OAK-D Lite, supplying a correct `.yaml` file as a [ROS parameter](https://github.com/slgrobotics/robots_bringup/blob/main/Docs/Sensors/OAK-D_Lite.md#configuration-files) worked well and lowered the FPS as intended.

If `i_fps` has no effect, the code in this package reads the pipeline as-is but publishes only _some_ of the data.  
This keeps the rates of the _Image_ and _PointCloud2_ topics acceptable for Wi-Fi-connected robots.

The package also runs the _publishers_ in a separate thread and restarts them if the device pipeline freezes.

We assume that your _OAK-D Lite_ camera is connected to the Raspberry Pi, while you view the data on your workstation (desktop PC).  
Both machines should run Ubuntu 24.04, and the Raspberry Pi can be “headless” (server version of the OS).

## Build

First, install standard Luxonis [ROS packages](https://github.com/luxonis/depthai-ros/tree/jazzy) on Raspberry Pi and on the Desktop machine :
```
sudo apt install ros-${ROS_DISTRO}-depthai-ros
```
Now you can clone and build this customized package on Raspberry Pi:
```
mkdir -p ~/depthai_ws/src
cd ~/depthai_ws/src
git clone https://github.com/slgrobotics/depthai_rospi.git
cd ~/depthai_ws
colcon build
```

## Usage

#### Stereo Publisher:

Here is how to launch the _stereo publisher node_ and its conversion helpers:
```
cd ~/depthai_ws
source install/setup.bash
ros2 launch depthai_rospi stereo_pi.launch.py
```
To see your data in RViz2 on the Desktop machine, follow this [guide](https://github.com/slgrobotics/robots_bringup/blob/main/Docs/Sensors/OAK-D_Lite.md#on-the-desktop-machine)

By default, the `fpsDivider` parameter is set to 10, which amounts to about 3 FPS on publishing all data (left and right Mono images and Depth).
All data is published raw (uncompressed).
The load on the WiFi link is around 120 Mbits/s for _PointCloud2_ data and about 8 Mbits/s for _Image_ data (run `nload wlan0` on RPi).

#### Spatial with 3D Detection:
Here is how to launch the *spatial_bb*, similar to an [example](https://github.com/slgrobotics/robots_bringup/blob/main/Docs/Sensors/OAK-D_Lite.md#spatial-examples) from *depthai_filters*:
```
ros2 launch depthai_rospi spatial_bb.launch.py
```
To see */overlay* and */oak/points* topics in RVis2, use settings here:
```
~/depthai_ws/src/depthai_rospi/rviz/default.rviz
```

## Useful Links

https://github.com/slgrobotics/robots_bringup/blob/main/Docs/Sensors/OAK-D_Lite.md

