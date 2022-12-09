![Cubes](cubes.gif)

# Overview

This project serves to demonstrate how you can write your own VR or XR application using OpenXR and Vulkan. These are the **OpenXR Vulkan Example**'s main features:

- Basic rendering of example scene to the headset, and into a resizeable mirror view on your desktop monitor.
- Focus on easy to read and understand C++ without smart pointers, inheritance, templates, etc.
- Warning-free code base spread over a small handful of classes.
- No OpenXR or Vulkan validation errors or warnings.
- CMake project setup for easy building.

Integrating both OpenXR and Vulkan yourself can be a daunting and painfully time-consuming task. Both APIs are very verbose and require the correct handling of many minute details. This is why there are two main use cases where the **OpenXR Vulkan Example** comes in handy:

- Fork the repository and use it as a starting point, saving yourself weeks of tedious integration work before you get to the juicy bits of VR or XR development.
- Reference the code while writing your own integration, to help you out if you are stuck with a problem, or for inspiration.


# Running the OpenXR Vulkan Example

1. Download the latest [release](https://github.com/janhsimon/openxr-vulkan-example/releases) (once available) or build the project yourself with the steps below.
2. Make sure your headset is connected to your computer and running or active.
3. Run the program!


# Building the OpenXR Vulkan Example

1. Install the [Vulkan SDK](https://vulkan.lunarg.com) version 1.3 or newer.
2. Install [CMake](https://cmake.org/download) version 3.1 or newer.
3. Clone the repository and generate build files.
4. Build!

The repository includes binaries for all dependencies except the Vulkan SDK on Windows. These can be found in the `external` folder. You will have to build these dependencies yourself on other platforms. Use the adress and version tag or commit hash in `version.txt` to ensure compatibility. Please don't hesitate to open a pull request if you have built dependencies for previously unsupported platforms.