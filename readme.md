![Teaser](teaser.gif)

# Overview

This project demonstrates how you can write your own VR application using OpenXR 1.1 and Vulkan 1.3. These are its main features:

- Basic rendering of example scene to the headset and into a resizeable mirror view on your desktop monitor. 
- Focus on easy to read and understand C++ without smart pointers, inheritance, templates, etc.
- Usage of the Vulkan `multiview` extension for extra performance.
- Warning-free code base spread over a small handful of classes.
- No OpenXR or Vulkan validation errors or warnings.
- CMake project setup for easy building.

Integrating both OpenXR and Vulkan yourself can be a daunting and painfully time-consuming task. Both APIs are very verbose and require the correct handling of countless minute details. This is why there are two main use cases where this project comes in handy:

1. Fork the repository and use it as a starting point to save yourself weeks of tedious integration work before you get to the juicy bits of VR development.
2. Reference the code while writing your own implementation from scratch, to help you out if you are stuck with a problem, or simply for inspiration.


# Running the OpenXR Vulkan Example

1. Download the latest [release](https://github.com/janhsimon/openxr-vulkan-example/releases) or build the project yourself with the steps below.
2. Make sure your headset is connected to your computer.
3. Run the program!


# Building the OpenXR Vulkan Example

1. Install the [Vulkan SDK](https://vulkan.lunarg.com) version 1.3 or newer.
2. Install [CMake](https://cmake.org/download) version 3.1 or newer.
3. Clone the repository and generate build files.
4. Build!

The repository includes binaries for all dependencies except the Vulkan SDK on Windows. These can be found in the `external` folder. You will have to build these dependencies yourself on other platforms. Use the address and version tag or commit hash in `version.txt` to ensure compatibility. Please don't hesitate to open a pull request if you have built dependencies for previously unsupported platforms.


# Attributions

| Asset | Title | Author | License |
| --- | --- | --- | --- |
| `models/Beetle.obj` | [Car Scene](https://sketchfab.com/3d-models/car-scene-b7b32eaca80d460c9338197e2c9d1408) | [toivo](https://sketchfab.com/toivo) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| `models/Bike.obj` | [Sci-fi Hover Bike 04](https://sketchfab.com/3d-models/sci-fi-hover-bike-04-a326ca10a01b4cb29a357eb23e5f6e01) | [taktelon](https://sketchfab.com/taktelon) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| `models/Car.obj` | [GAZ-21](https://sketchfab.com/3d-models/gaz-21-fdf5cbff00b04ab99a6ea3ab6b46533e) | [Ashkelon](https://sketchfab.com/Ashkelon) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| `models/Hand.obj` | [Hand Anatomy Reference](https://sketchfab.com/3d-models/hand-anatomy-reference-2024d77a872a45f1baf6d81b51fe18a6) | [Ant B-D](https://sketchfab.com/ant_bd) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| `models/Ruins.obj` | [Ancient Ruins Pack](https://sketchfab.com/3d-models/ancient-ruins-pack-4dad3b80562842f88d6568c5e1c469c2) | [Toni García Vilche](https://sketchfab.com/zul_gv) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |