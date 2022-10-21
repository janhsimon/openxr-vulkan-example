#pragma once

#include <vulkan/vulkan.h>

#include <vector>

struct GLFWwindow;
class Headset;

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
  bool render(uint32_t swapchainImageIndex);
  void present();

  Error getError() const;
  bool windowShouldClose() const;

private:
  Error error = Error::Success;

  const Headset* headset = nullptr;
  GLFWwindow* window = nullptr;
  VkSurfaceKHR surface = nullptr;
  VkQueue presentQueue = nullptr;
  VkSwapchainKHR swapchain = nullptr;
  std::vector<VkImage> swapchainImages;
  uint32_t destinationImageIndex = 0u;
  VkExtent2D renderSize = { 0u, 0u };
  bool resizeDetected = false;

  bool recreateSwapchain();
};