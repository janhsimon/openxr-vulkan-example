#pragma once

#include <glm/fwd.hpp>

#include <vulkan/vulkan.h>

#include <openxr/openxr.h>

#include <string>
#include <vector>

// All the things that can go wrong
enum class Error
{
  FeatureNotSupported,
  FileMissing,
  GenericGLFW,
  GenericOpenXR,
  GenericVulkan,
  HeadsetNotConnected,
  ModelLoadingFailure,
  OutOfMemory,
  VulkanNotSupported,
  WindowFailure
};

/*
 * The util namespaces offers a wide variety of useful utility functions.
 */
namespace util
{
// Reports an error with optional details through a system-native message box
void error(Error error, const std::string& details = "");

// Loads an OpenXR extension function by 'name' into 'function', returns false on error
bool loadXrExtensionFunction(XrInstance instance, const std::string& name, PFN_xrVoidFunction* function);

// Loads a Vulkan extension function by 'name', returns nullptr on error
PFN_vkVoidFunction loadVkExtensionFunction(VkInstance instance, const std::string& name);

// Unpacks a Vulkan or OpenXR extension list in a single string into a vector of c-style strings
std::vector<const char*> unpackExtensionString(const std::string& string);

// Loads a Vulkan shader from 'file' into 'shaderModule', returns false on error
bool loadShaderFromFile(VkDevice device, const std::string& filename, VkShaderModule& shaderModule);

// Finds a suitable Vulkan memory type index for given requirements and properties, returns false on error
bool findSuitableMemoryTypeIndex(VkPhysicalDevice physicalDevice,
                                 VkMemoryRequirements requirements,
                                 VkMemoryPropertyFlags properties,
                                 uint32_t& typeIndex);
// Aligns a value to an alignment
VkDeviceSize align(VkDeviceSize value, VkDeviceSize alignment);

// Creates an OpenXR path from a name string
XrPath stringToPath(XrInstance instance, const std::string& string);

// Creates an OpenXR action with a given names, returns false on error
bool createAction(XrActionSet actionSet,
                  const std::vector<XrPath>& paths,
                  const std::string& actionName,
                  const std::string& localizedActionName,
                  XrActionType type,
                  XrAction& action);

// Creates an OpenXR identity pose
XrPosef makeIdentity();

// Converts an OpenXR pose to a transformation matrix
glm::mat4 poseToMatrix(const XrPosef& pose);

// Creates an OpenXR projection matrix
glm::mat4 createProjectionMatrix(XrFovf fov, float nearClip, float farClip);

// Updates an action state for a given action and path in pose format, returns false on error
bool updateActionStatePose(XrSession session, XrAction action, XrPath path, XrActionStatePose& state);

// Updates an action state for a given action and path in float format, returns false on error
bool updateActionStateFloat(XrSession session, XrAction action, XrPath path, XrActionStateFloat& state);
} // namespace util