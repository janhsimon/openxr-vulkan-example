#pragma once

#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;
class Headset;
class RenderTarget;

class MirrorView final
{
public:
  enum class Error
  {
    Success,
    GLFW,
    Vulkan
  };

  MirrorView(const Headset* headset);

  void destroy() const; // Only call when construction succeeded

  void onWindowResize();

  void processWindowEvents() const;
  void render(VkImage sourceImage, VkExtent2D resolution);

  Error getError() const;
  bool windowShouldClose() const;

private:
  Error error = Error::Success;

  const Headset* headset = nullptr;
  GLFWwindow* window = nullptr;
  VkSurfaceKHR surface = nullptr;
  uint32_t drawQueueFamilyIndex = 0u, presentQueueFamilyIndex = 0u;
  VkQueue drawQueue = nullptr, presentQueue = nullptr;
  VkRenderPass renderPass = nullptr;
  VkSwapchainKHR swapchain = nullptr;
  std::vector<RenderTarget*> swapchainRenderTargets;
  VkCommandPool commandPool = nullptr;
  VkCommandBuffer commandBuffer = nullptr;
  VkSemaphore imageAvailableSemaphore = nullptr, renderFinishedSemaphore = nullptr;
  VkFence inFlightFence = nullptr;
  VkExtent2D renderSize = { 0u, 0u };
  bool resizeDetected = false;

  bool recreateSwapchain();
  void destroySwapchain() const;
};