#pragma once

#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;
class Headset;
class RenderTarget;

class MirrorView final
{
public:
  MirrorView(const Headset* headset);

  void destroy();

  void onWindowResize();

  void processWindowEvents() const;
  bool render(VkImage sourceImage, VkExtent2D resolution);

  bool isValid() const;
  bool windowShouldClose() const;

private:
  bool valid = true;

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
  void destroySwapchain();
};