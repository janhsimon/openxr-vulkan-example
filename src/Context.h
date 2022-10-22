#pragma once

#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

class Context final
{
public:
  enum class Error
  {
    Success,
    NoHeadsetDetected,
    GLFW,
    OpenXR,
    Vulkan
  };

  Context();

  void destroy() const; // Only call when construction succeeded

  Error getError() const;

  XrViewConfigurationType getXrViewType() const;
  XrInstance getXrInstance() const;
  XrSystemId getXrSystemId() const;

  VkInstance getVkInstance() const;
  VkPhysicalDevice getVkPhysicalDevice() const;

private:
  Error error = Error::Success;

  // OpenXR resources
  struct
  {
    // Extension function pointers
    PFN_xrGetVulkanInstanceExtensionsKHR getVulkanInstanceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsDeviceKHR getVulkanGraphicsDeviceKHR = nullptr;

    XrInstance instance = nullptr;
    XrSystemId systemId = 0u;

#ifdef DEBUG
    PFN_xrCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
    PFN_xrDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;
    XrDebugUtilsMessengerEXT debugUtilsMessenger = nullptr;
#endif
  } xr;

  // Vulkan resources
  struct
  {
    VkInstance instance = nullptr;
    VkPhysicalDevice physicalDevice = nullptr;

#ifdef DEBUG
    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;
    VkDebugUtilsMessengerEXT debugUtilsMessenger = nullptr;
#endif
  } vk;
};