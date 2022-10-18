#pragma once

#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/mat4x4.hpp>

#include <vector>

class RenderTarget;

class Headset final
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

  Headset();

  void sync() const;
  void destroy() const; // Only call when construction succeeded

  enum class BeginFrameResult
  {
    Error,       // An error occurred
    RenderFully, // Render this frame normally
    SkipRender,  // Skip rendering the frame but end it
    SkipFully    // Skip processing this frame entirely without ending it
  };
  BeginFrameResult beginFrame(uint32_t& swapchainImageIndex);
  void endFrame() const;

  Error getError() const;
  VkInstance getInstance() const;
  VkPhysicalDevice getPhysicalDevice() const;
  VkDevice getDevice() const;
  VkRenderPass getRenderPass() const;
  uint32_t getQueueFamilyIndex() const;
  VkQueue getQueue() const;
  size_t getEyeCount() const;
  VkExtent2D getEyeResolution(size_t eyeIndex) const;
  glm::mat4 getEyeViewMatrix(size_t eyeIndex) const;
  glm::mat4 getEyeProjectionMatrix(size_t eyeIndex) const;
  RenderTarget* getRenderTarget(size_t swapchainImageIndex) const;

private:
  Error error = Error::Success;

  size_t eyeCount = 0u;
  std::vector<glm::mat4> eyeViewMatrices;
  std::vector<glm::mat4> eyeProjectionMatrices;

  // OpenXR resources
  struct
  {
    // Extension function pointers
    PFN_xrGetVulkanInstanceExtensionsKHR getVulkanInstanceExtensionsKHR = nullptr;
    PFN_xrGetVulkanDeviceExtensionsKHR getVulkanDeviceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsDeviceKHR getVulkanGraphicsDeviceKHR = nullptr;
    PFN_xrGetVulkanGraphicsRequirementsKHR getVulkanGraphicsRequirementsKHR = nullptr;

    XrInstance instance = nullptr;
    XrSystemId systemId = 0u;
    XrSession session = nullptr;
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    XrSpace space = nullptr;
    XrFrameState frameState = {};
    XrViewState viewState = {};

    std::vector<XrViewConfigurationView> eyeImageInfos;
    std::vector<XrView> eyePoses;
    std::vector<XrCompositionLayerProjectionView> eyeRenderInfos;
    XrSwapchain swapchain = nullptr;
    std::vector<RenderTarget*> swapchainRenderTargets;

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
    uint32_t queueFamilyIndex = 0u;
    VkDevice device = nullptr;
    VkQueue queue = nullptr;
    VkRenderPass renderPass = nullptr;

#ifdef DEBUG
    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;
    VkDebugUtilsMessengerEXT debugUtilsMessenger = nullptr;
#endif
  } vk;
};