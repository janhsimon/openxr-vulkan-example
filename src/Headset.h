#pragma once

#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/mat4x4.hpp>

#include <vector>

class Context;
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

  Headset(const Context* context, VkSurfaceKHR mirrorSurface);

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
  void submit() const;
  void endFrame() const;

  Error getError() const;
  VkDevice getDevice() const;
  VkRenderPass getRenderPass() const;
  VkQueue getDrawQueue() const;
  VkQueue getPresentQueue() const;
  VkCommandBuffer getCommandBuffer() const;
  VkSemaphore getImageAvailableSemaphore() const;
  VkSemaphore getRenderFinishedSemaphore() const;
  size_t getEyeCount() const;
  VkExtent2D getEyeResolution(size_t eyeIndex) const;
  glm::mat4 getEyeViewMatrix(size_t eyeIndex) const;
  glm::mat4 getEyeProjectionMatrix(size_t eyeIndex) const;
  RenderTarget* getRenderTarget(size_t swapchainImageIndex) const;

private:
  Error error = Error::Success;

  const Context* context = nullptr;

  size_t eyeCount = 0u;
  std::vector<glm::mat4> eyeViewMatrices;
  std::vector<glm::mat4> eyeProjectionMatrices;

  // OpenXR resources
  struct
  {
    // Extension function pointers
    PFN_xrGetVulkanDeviceExtensionsKHR getVulkanDeviceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsRequirementsKHR getVulkanGraphicsRequirementsKHR = nullptr;

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
  } xr;

  // Vulkan resources
  struct
  {
    uint32_t drawQueueFamilyIndex = 0u, presentQueueFamilyIndex = 0u;
    VkDevice device = nullptr;
    VkQueue drawQueue = nullptr, presentQueue = nullptr;
    VkRenderPass renderPass = nullptr;
    VkImage depthImage = nullptr;
    VkDeviceMemory depthMemory = nullptr;
    VkImageView depthImageView = nullptr;
    VkCommandPool commandPool = nullptr;
    VkCommandBuffer commandBuffer = nullptr;
    VkSemaphore imageAvailableSemaphore = nullptr, renderFinishedSemaphore = nullptr;
    VkFence inFlightFence = nullptr;
  } vk;

  bool onSessionStateReady() const;
  bool onSessionStateStopping() const;
  bool onSessionStateExiting() const;
};