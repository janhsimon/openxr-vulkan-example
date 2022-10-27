set(PROJECT_NAME openxr-vulkan-example)

cmake_minimum_required(VERSION 3.1)
project(${PROJECT_NAME})

include(FindVulkan)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)
add_subdirectory(src)