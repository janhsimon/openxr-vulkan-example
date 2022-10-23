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

  bool createDevice(VkSurfaceKHR mirrorSurface); // Only call when construction succeeded
  void sync() const;
  void destroy() const; // Only call when construction and createDevice() succeeded

  Error getError() const;

  XrViewConfigurationType getXrViewType() const;
  XrInstance getXrInstance() const;
  XrSystemId getXrSystemId() const;

  VkInstance getVkInstance() const;
  VkPhysicalDevice getVkPhysicalDevice() const;
  uint32_t getVkDrawQueueFamilyIndex() const;
  VkDevice getVkDevice() const;
  VkQueue getVkDrawQueue() const;
  VkQueue getVkPresentQueue() const;

private:
  Error error = Error::Success;

  // OpenXR resources
  struct
  {
    // Extension function pointers
    PFN_xrGetVulkanInstanceExtensionsKHR getVulkanInstanceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsDeviceKHR getVulkanGraphicsDeviceKHR = nullptr;
    PFN_xrGetVulkanDeviceExtensionsKHR getVulkanDeviceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsRequirementsKHR getVulkanGraphicsRequirementsKHR = nullptr;

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
    uint32_t drawQueueFamilyIndex = 0u, presentQueueFamilyIndex = 0u;
    VkDevice device = nullptr;
    VkQueue drawQueue = nullptr, presentQueue = nullptr;

#ifdef DEBUG
    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;
    VkDebugUtilsMessengerEXT debugUtilsMessenger = nullptr;
#endif
  } vk;
};