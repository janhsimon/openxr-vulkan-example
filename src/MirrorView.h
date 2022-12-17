#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class Context;
struct GLFWwindow;
class Headset;
class Renderer;

/*
 * The mirror view class handles the creation, updating, resizing, and eventual closing of the desktop window that shows
 * a copy of what is rendered into the headset. It depends on GLFW for handling the operating system, and Vulkan for the
 * blitting into the window surface.
 */
class MirrorView final
{
public:
  MirrorView(const Context* context);
  ~MirrorView();

  void onWindowResize();

  bool connect(const Headset* headset, const Renderer* renderer);
  void processWindowEvents() const;

  enum class RenderResult
  {
    Error,    // An error occurred
    Visible,  // Visible mirror view for normal rendering
    Invisible // Minimized window for example without rendering
  };
  RenderResult render(uint32_t swapchainImageIndex);
  void present();

  bool isValid() const;
  bool isExitRequested() const;
  VkSurfaceKHR getSurface() const;

private:
  bool valid = true;

  const Context* context = nullptr;
  const Headset* headset = nullptr;
  const Renderer* renderer = nullptr;

  GLFWwindow* window = nullptr;

  VkSurfaceKHR surface = nullptr;
  VkSwapchainKHR swapchain = nullptr;
  std::vector<VkImage> swapchainImages;
  VkExtent2D swapchainResolution = { 0u, 0u };

  uint32_t destinationImageIndex = 0u;
  bool resizeDetected = false;

  bool recreateSwapchain();
};