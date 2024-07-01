#pragma once

#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

/*
 * The context class handles the initial loading of both OpenXR and Vulkan base functionality such as instances, OpenXR
 * sessions, Vulkan devices and queues, and so on. It also loads debug utility messengers for both OpenXR and Vulkan if
 * the preprocessor macro DEBUG is defined. This enables console output that is crucial to finding potential issues in
 * OpenXR or Vulkan.
 */
class Context final
{
public:
  Context();
  ~Context();

  bool createDevice(VkSurfaceKHR mirrorSurface);
  void sync() const;

  bool isValid() const;

  XrViewConfigurationType getXrViewType() const;
  XrInstance getXrInstance() const;
  XrSystemId getXrSystemId() const;

  VkInstance getVkInstance() const;
  VkPhysicalDevice getVkPhysicalDevice() const;
  uint32_t getVkDrawQueueFamilyIndex() const;
  VkDevice getVkDevice() const;
  VkQueue getVkDrawQueue() const;
  VkQueue getVkPresentQueue() const;

  VkDeviceSize getUniformBufferOffsetAlignment() const;
  VkSampleCountFlagBits getMultisampleCount() const;

private:
  bool valid = true;

  // Extension function pointers
  PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR = nullptr;
  PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR = nullptr;
  PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR = nullptr;
  PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = nullptr;

  XrInstance xrInstance = nullptr;
  XrSystemId systemId = 0u;

  VkInstance vkInstance = nullptr;
  VkPhysicalDevice physicalDevice = nullptr;
  uint32_t drawQueueFamilyIndex = 0u, presentQueueFamilyIndex = 0u;
  VkDevice device = nullptr;
  VkQueue drawQueue = nullptr, presentQueue = nullptr;
  VkDeviceSize uniformBufferOffsetAlignment = 0u;
  VkSampleCountFlagBits multisampleCount = VK_SAMPLE_COUNT_1_BIT;

#ifdef DEBUG
  PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT = nullptr;
  PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT = nullptr;
  XrDebugUtilsMessengerEXT xrDebugUtilsMessenger = nullptr;

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
  VkDebugUtilsMessengerEXT vkDebugUtilsMessenger = nullptr;
#endif
};