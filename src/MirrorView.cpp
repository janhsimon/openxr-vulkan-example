#include "MirrorView.h"

#include "Context.h"
#include "Headset.h"
#include "RenderTarget.h"
#include "Renderer.h"
#include "Util.h"

#include <glfw/glfw3.h>

#include <glm/common.hpp>

#include <array>
#include <sstream>

namespace
{
constexpr const char* windowTitle = "OpenXR Vulkan Example";
constexpr VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
constexpr size_t mirrorEyeIndex = 1u; // Eye index to mirror, 0 = left, 1 = right

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  MirrorView* mirrorView = reinterpret_cast<MirrorView*>(glfwGetWindowUserPointer(window));
  mirrorView->onWindowResize();
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (action == GLFW_RELEASE && key == GLFW_KEY_ESCAPE)
  {
    glfwSetWindowShouldClose(window, 1);
  }
}
} // namespace

MirrorView::MirrorView(const Context* context) : context(context)
{
  // Create a fullscreen window
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();

  int width, height;
  glfwGetMonitorWorkarea(monitor, nullptr, nullptr, &width, &height);

#ifdef DEBUG
  // Create a quarter-sized window in debug mode instead
  width /= 2;
  height /= 2;
  monitor = nullptr;
#endif

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(width, height, windowTitle, monitor, nullptr);
  if (!window)
  {
    std::stringstream s;
    s << width << "x" << height << (monitor ? " fullscreen" : " windowed");
    util::error(Error::WindowFailure, s.str());
    valid = false;
    return;
  }

  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  glfwSetKeyCallback(window, keyCallback);

  // Hide the mouse cursor
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

  // Create a surface for the window
  VkResult result = glfwCreateWindowSurface(context->getVkInstance(), window, nullptr, &surface);
  if (result != VK_SUCCESS)
  {
    util::error(Error::GenericGLFW);
    valid = false;
    return;
  }
}

MirrorView::~MirrorView()
{
  const VkDevice device = context->getVkDevice();
  if (device && swapchain)
  {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }

  const VkInstance instance = context->getVkInstance();
  if (instance && surface)
  {
    vkDestroySurfaceKHR(instance, surface, nullptr);
  }

  if (window)
  {
    glfwDestroyWindow(window);
  }

  glfwTerminate();
}

void MirrorView::onWindowResize()
{
  resizeDetected = true;
}

bool MirrorView::connect(const Headset* headset, const Renderer* renderer)
{
  this->headset = headset;
  this->renderer = renderer;

  if (!recreateSwapchain())
  {
    return false;
  }

  return true;
}

void MirrorView::processWindowEvents() const
{
  glfwPollEvents();
}

MirrorView::RenderResult MirrorView::render(uint32_t swapchainImageIndex)
{
  if (swapchainResolution.width == 0u || swapchainResolution.height == 0u)
  {
    // Just check for maximizing as long as the window is minimized
    if (resizeDetected)
    {
      resizeDetected = false;
      if (!recreateSwapchain())
      {
        return RenderResult::Error;
      }
    }
    else
    {
      // Otherwise skip minimized frames
      return RenderResult::Invisible;
    }
  }

  const VkResult result =
    vkAcquireNextImageKHR(context->getVkDevice(), swapchain, UINT64_MAX, renderer->getCurrentDrawableSemaphore(),
                          VK_NULL_HANDLE, &destinationImageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // Recreate the swapchain and then stop rendering this frame as it is out of date already
    if (!recreateSwapchain())
    {
      return RenderResult::Error;
    }

    return RenderResult::Invisible;
  }
  else if (result != VK_SUBOPTIMAL_KHR && result != VK_SUCCESS)
  {
    // Treat a suboptimal like a successful frame
    return RenderResult::Invisible;
  }

  const VkCommandBuffer commandBuffer = renderer->getCurrentCommandBuffer();
  const VkImage sourceImage = headset->getRenderTarget(swapchainImageIndex)->getImage(); // OpenXR swapchain image
  const VkImage destinationImage = swapchainImages.at(destinationImageIndex);            // Mirror view swapchain image

  // Transition the layer of the OpenXR swapchain image that is to be mirrored in the mirror view to the transfer source
  // optimal layout and transition the mirror view swapchain image to the transfer destination optimal layout. Also
  // ensure that all color attachment write access in the color attachment output stage has concluded in the OpenXR
  // swapchain image before allowing any transfer read access in the transfer stage.
  {
    VkImageMemoryBarrier sourceImageMemoryBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    sourceImageMemoryBarrier.image = sourceImage;
    sourceImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    sourceImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    sourceImageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    sourceImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    sourceImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    sourceImageMemoryBarrier.subresourceRange.layerCount = 1u;
    sourceImageMemoryBarrier.subresourceRange.baseArrayLayer = mirrorEyeIndex;
    sourceImageMemoryBarrier.subresourceRange.levelCount = 1u;
    sourceImageMemoryBarrier.subresourceRange.baseMipLevel = 0u;

    VkImageMemoryBarrier destinationImageMemoryBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    destinationImageMemoryBarrier.image = destinationImage;
    destinationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    destinationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    destinationImageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
    destinationImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    destinationImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    destinationImageMemoryBarrier.subresourceRange.layerCount = 1u;
    destinationImageMemoryBarrier.subresourceRange.baseArrayLayer = 0u;
    destinationImageMemoryBarrier.subresourceRange.levelCount = 1u;
    destinationImageMemoryBarrier.subresourceRange.baseMipLevel = 0u;

    const std::array imageMemoryBarriers = { sourceImageMemoryBarrier, destinationImageMemoryBarrier };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 0u, nullptr,
                         static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
  }

  // We need to crop the source image region to preserve the aspect ratio of the mirror view window
  const VkExtent2D eyeResolution = headset->getEyeResolution(mirrorEyeIndex);
  const glm::vec2 sourceResolution = { static_cast<float>(eyeResolution.width),
                                       static_cast<float>(eyeResolution.height) };
  const float sourceAspectRatio = sourceResolution.x / sourceResolution.y;
  const glm::vec2 destinationResolution = { static_cast<float>(swapchainResolution.width),
                                            static_cast<float>(swapchainResolution.height) };
  const float destinationAspectRatio = destinationResolution.x / destinationResolution.y;
  glm::vec2 cropResolution = sourceResolution, cropOffset = { 0.0f, 0.0f };

  if (sourceAspectRatio < destinationAspectRatio)
  {
    cropResolution.y = sourceResolution.x / destinationAspectRatio;
    cropOffset.y = (sourceResolution.y - cropResolution.y) / 2.0f;
  }
  else if (sourceAspectRatio > destinationAspectRatio)
  {
    cropResolution.x = sourceResolution.y * destinationAspectRatio;
    cropOffset.x = (sourceResolution.x - cropResolution.x) / 2.0f;
  }

  // Blit the source to the destination image
  VkImageBlit imageBlit{};
  imageBlit.srcOffsets[0] = { static_cast<int32_t>(cropOffset.x), static_cast<int32_t>(cropOffset.y), 0 };
  imageBlit.srcOffsets[1] = { static_cast<int32_t>(cropOffset.x + cropResolution.x),
                              static_cast<int32_t>(cropOffset.y + cropResolution.y), 1 };
  imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBlit.srcSubresource.mipLevel = 0u;
  imageBlit.srcSubresource.baseArrayLayer = mirrorEyeIndex;
  imageBlit.srcSubresource.layerCount = 1u;

  imageBlit.dstOffsets[0] = { 0, 0, 0 };
  imageBlit.dstOffsets[1] = { static_cast<int32_t>(destinationResolution.x),
                              static_cast<int32_t>(destinationResolution.y), 1 };
  imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBlit.dstSubresource.layerCount = 1u;
  imageBlit.dstSubresource.baseArrayLayer = 0u;
  imageBlit.dstSubresource.mipLevel = 0u;

  vkCmdBlitImage(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destinationImage,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &imageBlit, VK_FILTER_NEAREST);

  // Transition the layer of the OpenXR swapchain image that was mirrored to the mirror view back to the color
  // attachment optimal layout and transition the mirror view swapchain image to the present source layout. Also ensure
  // that all transfer read access in the transfer stage has concluded for the OpenXR swapchain image before allowing
  // any transfer read access in the transfer stage.
  {
    VkImageMemoryBarrier sourceImageMemoryBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    sourceImageMemoryBarrier.image = sourceImage;
    sourceImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    sourceImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    sourceImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    sourceImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
    sourceImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    sourceImageMemoryBarrier.subresourceRange.layerCount = 1u;
    sourceImageMemoryBarrier.subresourceRange.baseArrayLayer = mirrorEyeIndex;
    sourceImageMemoryBarrier.subresourceRange.levelCount = 1u;
    sourceImageMemoryBarrier.subresourceRange.baseMipLevel = 0u;

    VkImageMemoryBarrier destinationImageMemoryBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    destinationImageMemoryBarrier.image = destinationImage;
    destinationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    destinationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    destinationImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    destinationImageMemoryBarrier.dstAccessMask = VK_ACCESS_NONE;
    destinationImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    destinationImageMemoryBarrier.subresourceRange.layerCount = 1u;
    destinationImageMemoryBarrier.subresourceRange.baseArrayLayer = 0u;
    destinationImageMemoryBarrier.subresourceRange.levelCount = 1u;
    destinationImageMemoryBarrier.subresourceRange.baseMipLevel = 0u;

    const std::array imageMemoryBarriers = { sourceImageMemoryBarrier, destinationImageMemoryBarrier };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_DEPENDENCY_BY_REGION_BIT, 0u, nullptr, 0u, nullptr,
                         static_cast<uint32_t>(imageMemoryBarriers.size()), imageMemoryBarriers.data());
  }

  return RenderResult::Visible;
}

void MirrorView::present()
{
  const VkSemaphore presentableSemaphore = renderer->getCurrentPresentableSemaphore();

  VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  presentInfo.waitSemaphoreCount = 1u;
  presentInfo.pWaitSemaphores = &presentableSemaphore;
  presentInfo.swapchainCount = 1u;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &destinationImageIndex;

  const VkResult result = vkQueuePresentKHR(context->getVkPresentQueue(), &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
  {
    // Recreate the swapchain for the next frame if necessary
    if (!recreateSwapchain())
    {
      return;
    }
  }
  else if (result != VK_SUCCESS)
  {
    return;
  }
}

bool MirrorView::isValid() const
{
  return valid;
}

bool MirrorView::isExitRequested() const
{
  return static_cast<bool>(glfwWindowShouldClose(window));
}

VkSurfaceKHR MirrorView::getSurface() const
{
  return surface;
}

bool MirrorView::recreateSwapchain()
{
  context->sync();

  const VkPhysicalDevice physicalDevice = context->getVkPhysicalDevice();

  // Get the surface capabilities and extent
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  {
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      return false;
    }

    if (!(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
      util::error(Error::GenericVulkan);
      return false;
    }

    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() &&
        surfaceCapabilities.currentExtent.height != std::numeric_limits<uint32_t>::max())
    {
      // Use any valid extent
      swapchainResolution = surfaceCapabilities.currentExtent;
    }
    else
    {
      // Find the closest extent to use instead of an invalid extent
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      swapchainResolution.width = glm::clamp(width, static_cast<int>(surfaceCapabilities.minImageExtent.width),
                                             static_cast<int>(surfaceCapabilities.maxImageExtent.width));
      swapchainResolution.height = glm::clamp(height, static_cast<int>(surfaceCapabilities.minImageExtent.height),
                                              static_cast<int>(surfaceCapabilities.maxImageExtent.height));
    }

    // Skip the rest if the window was minimized
    if (swapchainResolution.width == 0u || swapchainResolution.height == 0u)
    {
      return true;
    }
  }

  // Get the surface formats and pick one with the desired color format support
  VkSurfaceFormatKHR surfaceFormat;
  {
    uint32_t surfaceFormatCount = 0u;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr) != VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      return false;
    }

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data()) !=
        VK_SUCCESS)
    {
      util::error(Error::GenericVulkan);
      return false;
    }

    // Find the surface format to use
    bool surfaceFormatFound = false;
    for (const VkSurfaceFormatKHR& surfaceFormatCandidate : surfaceFormats)
    {
      if (surfaceFormatCandidate.format == colorFormat)
      {
        surfaceFormat = surfaceFormatCandidate;
        surfaceFormatFound = true;
        break;
      }
    }

    if (!surfaceFormatFound)
    {
      util::error(Error::FeatureNotSupported, "Vulkan swapchain color format");
      return false;
    }
  }

  const VkDevice vkDevice = context->getVkDevice();

  // Clean up before recreating the swapchain and render targets
  if (swapchain)
  {
    vkDestroySwapchainKHR(vkDevice, swapchain, nullptr);
  }

  // Create a new swapchain
  VkSwapchainCreateInfoKHR swapchainCreateInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
  swapchainCreateInfo.surface = surface;
  swapchainCreateInfo.presentMode = presentMode;
  swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1u;
  swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapchainCreateInfo.imageFormat = surfaceFormat.format;
  swapchainCreateInfo.imageExtent = swapchainResolution;
  swapchainCreateInfo.imageArrayLayers = 1u;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (vkCreateSwapchainKHR(vkDevice, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  // Retrieve the new swapchain images
  uint32_t swapchainImageCount = 0u;
  if (vkGetSwapchainImagesKHR(vkDevice, swapchain, &swapchainImageCount, nullptr) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  swapchainImages.resize(swapchainImageCount);
  if (vkGetSwapchainImagesKHR(vkDevice, swapchain, &swapchainImageCount, swapchainImages.data()) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  return true;
}