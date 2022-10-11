#include "MirrorView.h"

#include "Headset.h"
#include "RenderTarget.h"

#include <glfw/glfw3.h>

#include <iostream>

namespace
{
inline constexpr VkExtent2D windowSize = { 640, 480 };
inline constexpr const char* windowTitle = "xrvk";
inline constexpr VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
inline constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  MirrorView* mirrorView = reinterpret_cast<MirrorView*>(glfwGetWindowUserPointer(window));
  mirrorView->onWindowResize();
}
} // namespace

MirrorView::MirrorView(const Headset* headset) : headset(headset)
{
  if (!glfwInit())
  {
    std::cerr << "Failed to initialize GLFW\n";
    valid = false;
    return;
  }

  if (!glfwVulkanSupported())
  {
    std::cerr << "Vulkan not supported\n";
    valid = false;
    return;
  }

  // Create fullscreen window
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();

  int x, y, width, height;
  glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);

#ifdef DEBUG
  // Create quarter-sized window in debug mode instead
  width /= 2;
  height /= 2;
  monitor = nullptr;
#endif

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(width, height, windowTitle, monitor, nullptr);

  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

  // Create surface for the window
  VkResult result = glfwCreateWindowSurface(headset->getInstance(), window, nullptr, &surface);
  if (result != VK_SUCCESS)
  {
    std::cerr << "Failed to create window surface\n";
    valid = false;
    return;
  }

  VkPhysicalDevice physicalDevice = headset->getPhysicalDevice();

  // Pick queue family indices for the physical device
  {
    // Retrieve queue families
    std::vector<VkQueueFamilyProperties> queueFamilies;
    uint32_t queueFamilyCount = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0u)
    {
      std::cerr << "Found zero queue families for physical device\n";
      valid = false;
      return;
    }

    queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    // Pick queue family indices
    bool drawQueueFamilyIndexFound = false;
    bool presentQueueFamilyIndexFound = false;
    for (size_t queueFamilyIndexCandidate = 0u; queueFamilyIndexCandidate < queueFamilies.size();
         ++queueFamilyIndexCandidate)
    {
      const VkQueueFamilyProperties& queueFamilyCandidate = queueFamilies.at(queueFamilyIndexCandidate);

      // Check that queue family includes actual queues
      if (queueFamilyCandidate.queueCount == 0u)
      {
        continue;
      }

      // Check queue family for drawing support
      if (!drawQueueFamilyIndexFound && queueFamilyCandidate.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        drawQueueFamilyIndex = static_cast<uint32_t>(queueFamilyIndexCandidate);
        drawQueueFamilyIndexFound = true;
      }

      // Check queue family for presenting support
      VkBool32 presentSupport = false;
      if (vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, static_cast<uint32_t>(queueFamilyIndexCandidate),
                                               surface, &presentSupport) != VK_SUCCESS)
      {
        std::cerr << "Failed to determine queue family surface support for physical device\n";
        valid = false;
        return;
      }

      if (!presentQueueFamilyIndexFound && presentSupport)
      {
        presentQueueFamilyIndex = static_cast<uint32_t>(queueFamilyIndexCandidate);
        presentQueueFamilyIndexFound = true;
      }

      // Stop looking if both queue families are found
      if (drawQueueFamilyIndexFound && presentQueueFamilyIndexFound)
      {
        break;
      }
    }

    if (!drawQueueFamilyIndexFound || !presentQueueFamilyIndexFound)
    {
      std::cerr << "Required queue families not supported for physical device\n";
      valid = false;
      return;
    }
  }

  const VkDevice device = headset->getDevice();
  vkGetDeviceQueue(device, drawQueueFamilyIndex, 0u, &drawQueue);
  vkGetDeviceQueue(device, presentQueueFamilyIndex, 0u, &presentQueue);

  // Create render pass
  {
    VkAttachmentDescription colorAttachmentDescription{};
    colorAttachmentDescription.format = colorFormat;
    colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0u;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1u;
    subpassDescription.pColorAttachments = &colorAttachmentReference;

    VkRenderPassCreateInfo renderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassCreateInfo.attachmentCount = 1u;
    renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
    renderPassCreateInfo.subpassCount = 1u;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
      std::cerr << "Failed to create render pass\n";
      valid = false;
      return;
    }
  }

  // Create swapchain and render targets
  if (!recreateSwapchain())
  {
    valid = false;
    return;
  }

  // Create command pool
  VkCommandPoolCreateInfo commandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolCreateInfo.queueFamilyIndex = drawQueueFamilyIndex;
  if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
  {
    std::cerr << "Failed to create command pool\n";
    valid = false;
    return;
  }

  // Allocate command buffer
  VkCommandBufferAllocateInfo commandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  commandBufferAllocateInfo.commandPool = commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = 1u;
  if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer) != VK_SUCCESS)
  {
    std::cerr << "Failed to allocate command buffers\n";
    valid = false;
    return;
  }

  // Create semaphores
  VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS)
  {
    std::cerr << "Failed to create semaphore\n";
    valid = false;
    return;
  }

  if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS)
  {
    std::cerr << "Failed to create semaphore\n";
    valid = false;
    return;
  }

  // Create memory fence
  VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  if (vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFence) != VK_SUCCESS)
  {
    std::cerr << "Failed to create memory fence\n";
    valid = false;
    return;
  }
}

void MirrorView::destroy()
{
  if (!valid)
  {
    return;
  }
  valid = false;

  const VkDevice device = headset->getDevice();
  vkDestroyFence(device, inFlightFence, nullptr);
  vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
  vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
  vkDestroyCommandPool(device, commandPool, nullptr);

  destroySwapchain();

  vkDestroyRenderPass(device, renderPass, nullptr);
  vkDestroySurfaceKHR(headset->getInstance(), surface, nullptr);

  glfwDestroyWindow(window);
  glfwTerminate();
}

void MirrorView::onWindowResize()
{
  resizeDetected = true;
}

void MirrorView::processWindowEvents() const
{
  glfwPollEvents();
}

bool MirrorView::render(VkImage sourceImage, VkExtent2D resolution)
{
  const VkDevice device = headset->getDevice();

  if (vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
  {
    std::cerr << "Failed to wait for mirror view memory fences\n";
    return false;
  }

  if (renderSize.width == 0u || renderSize.height == 0u)
  {
    // Just check for maximizing as long as the window is minimized
    if (resizeDetected)
    {
      resizeDetected = false;
      if (!recreateSwapchain())
      {
        return false;
      }
    }
    else
    {
      // Otherwise skip minized frames
      return true;
    }
  }

  uint32_t destinationImageIndex;
  VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE,
                                          &destinationImageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // Recreate tswapchain and then stop rendering this frame as it is out of date already
    return recreateSwapchain();
  }
  else if (result != VK_SUBOPTIMAL_KHR && result != VK_SUCCESS)
  {
    // Just continue in case of a suboptimal
    std::cerr << "Failed to acquire mirror view swapchain image\n";
    return false;
  }

  if (vkResetFences(device, 1, &inFlightFence) != VK_SUCCESS)
  {
    std::cerr << "Failed to reset mirror view memory fence\n";
    return false;
  }

  // Reset and begin recording command buffer
  if (vkResetCommandBuffer(commandBuffer, 0u) != VK_SUCCESS)
  {
    std::cerr << "Failed to reset mirror view command buffer\n";
    return false;
  }

  VkCommandBufferBeginInfo commandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
  {
    std::cerr << "Failed to begin mirror view command buffer recording\n";
    return false;
  }

  VkImage destinationImage = swapchainRenderTargets.at(destinationImageIndex)->getImage();

  // Convert source image layout from undefined to transfer source
  VkImageMemoryBarrier imageMemoryBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  imageMemoryBarrier.image = sourceImage;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  imageMemoryBarrier.srcAccessMask = 0u;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageMemoryBarrier.subresourceRange.baseArrayLayer = 0u;
  imageMemoryBarrier.subresourceRange.baseMipLevel = 0u;
  imageMemoryBarrier.subresourceRange.layerCount = 1u;
  imageMemoryBarrier.subresourceRange.levelCount = 1u;
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr,
                       0u, nullptr, 1u, &imageMemoryBarrier);

  // Convert destination image layout from undefined to transfer destination
  imageMemoryBarrier.image = destinationImage;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageMemoryBarrier.srcAccessMask = 0u;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageMemoryBarrier.subresourceRange.baseArrayLayer = 0u;
  imageMemoryBarrier.subresourceRange.baseMipLevel = 0u;
  imageMemoryBarrier.subresourceRange.layerCount = 1u;
  imageMemoryBarrier.subresourceRange.levelCount = 1u;
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr,
                       0u, nullptr, 1u, &imageMemoryBarrier);

  // Blit source to destination
  const VkExtent2D mirrorSize = renderSize;

  VkImageBlit imageBlit{};
  imageBlit.srcOffsets[1] = { static_cast<int32_t>(resolution.width), static_cast<int32_t>(resolution.height), 1 };
  imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBlit.srcSubresource.mipLevel = 0u;
  imageBlit.srcSubresource.baseArrayLayer = 0u;
  imageBlit.srcSubresource.layerCount = 1u;
  imageBlit.dstOffsets[1] = { static_cast<int32_t>(mirrorSize.width), static_cast<int32_t>(mirrorSize.height), 1 };
  imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBlit.dstSubresource.mipLevel = 0u;
  imageBlit.dstSubresource.baseArrayLayer = 0u;
  imageBlit.dstSubresource.layerCount = 1u;
  vkCmdBlitImage(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destinationImage,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_NEAREST);

  // Convert destination image layout from transfer destination to present
  imageMemoryBarrier.image = destinationImage;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = 0u;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageMemoryBarrier.subresourceRange.baseArrayLayer = 0u;
  imageMemoryBarrier.subresourceRange.baseMipLevel = 0u;
  imageMemoryBarrier.subresourceRange.layerCount = 1u;
  imageMemoryBarrier.subresourceRange.levelCount = 1u;
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr,
                       0u, nullptr, 1u, &imageMemoryBarrier);

  // Convert source image layout from transfer source to color attachment
  imageMemoryBarrier.image = sourceImage;
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageMemoryBarrier.subresourceRange.baseArrayLayer = 0u;
  imageMemoryBarrier.subresourceRange.baseMipLevel = 0u;
  imageMemoryBarrier.subresourceRange.layerCount = 1u;
  imageMemoryBarrier.subresourceRange.levelCount = 1u;
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
                       0u, nullptr, 0u, nullptr, 1u, &imageMemoryBarrier);

  // End recording command buffer
  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
  {
    std::cerr << "Failed to end mirror view command buffer recording\n";
    return false;
  }

  // Render
  VkPipelineStageFlags waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.waitSemaphoreCount = 1u;
  submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
  submitInfo.pWaitDstStageMask = &waitStages;
  submitInfo.commandBufferCount = 1u;
  submitInfo.pCommandBuffers = &commandBuffer;
  submitInfo.signalSemaphoreCount = 1u;
  submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
  if (vkQueueSubmit(drawQueue, 1u, &submitInfo, inFlightFence) != VK_SUCCESS)
  {
    std::cerr << "Failed to submit mirror view graphics queue\n";
    return false;
  }

  // Present
  VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  presentInfo.waitSemaphoreCount = 1u;
  presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
  presentInfo.swapchainCount = 1u;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &destinationImageIndex;

  result = vkQueuePresentKHR(presentQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
  {
    // Recreate swapchain for the next frame if necessary
    if (!recreateSwapchain())
    {
      return false;
    }
  }
  else if (result != VK_SUCCESS)
  {
    std::cerr << "Failed to present mirror view\n";
    return false;
  }

  return true;
}

bool MirrorView::isValid() const
{
  return valid;
}

bool MirrorView::windowShouldClose() const
{
  return glfwWindowShouldClose(window);
}

bool MirrorView::recreateSwapchain()
{
  const VkDevice device = headset->getDevice();
  vkDeviceWaitIdle(device);

  const VkPhysicalDevice physicalDevice = headset->getPhysicalDevice();

  // Get surface capabilities and extent
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  {
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) != VK_SUCCESS)
    {
      std::cerr << "Failed to get surface capabilities of physical device\n";
      return false;
    }

    if (!(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
    {
      std::cerr << "Surface does not supported transfer usage\n";
      return false;
    }

    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() &&
        surfaceCapabilities.currentExtent.height != std::numeric_limits<uint32_t>::max())
    {
      // Use any valid extent
      renderSize = surfaceCapabilities.currentExtent;
    }
    else
    {
      // Find the closest extent to use instead of an invalid extent
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      renderSize.width = glm::clamp(width, static_cast<int>(surfaceCapabilities.minImageExtent.width),
                                    static_cast<int>(surfaceCapabilities.maxImageExtent.width));
      renderSize.height = glm::clamp(height, static_cast<int>(surfaceCapabilities.minImageExtent.height),
                                     static_cast<int>(surfaceCapabilities.maxImageExtent.height));
    }

    // Skip the rest if the window was minimized
    if (renderSize.width == 0u || renderSize.height == 0u)
    {
      return true;
    }
  }

  // Get surface formats and pick one with the desired color format support
  VkSurfaceFormatKHR surfaceFormat;
  {
    uint32_t surfaceFormatCount = 0u;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr) != VK_SUCCESS)
    {
      std::cerr << "Failed to enumerate surface formats for physical device\n";
      return false;
    }

    if (surfaceFormatCount == 0u)
    {
      std::cerr << "Found zero surface formats for physical device\n";
      return false;
    }

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data()) !=
        VK_SUCCESS)
    {
      std::cerr << "Failed to retrieve surface formats for physical device\n";
      return false;
    }

    // Find surface format to use
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
      std::cerr << "Required surface format not found\n";
      return false;
    }
  }

  // Clean up before recreating the swapchain and render targets
  destroySwapchain();

  // Create new swapchain
  VkSwapchainCreateInfoKHR swapchainCreateInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
  swapchainCreateInfo.surface = surface;
  swapchainCreateInfo.presentMode = presentMode;
  swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1u;
  swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapchainCreateInfo.imageFormat = surfaceFormat.format;
  swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
  swapchainCreateInfo.imageArrayLayers = 1u;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
  {
    std::cerr << "Failed to create mirror view swapchain\n";
    return false;
  }

  // Retrieve new swapchain images
  std::vector<VkImage> swapchainImages;
  uint32_t swapchainImageCount = 0u;
  if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr) != VK_SUCCESS)
  {
    std::cerr << "Failed to enumerate mirror view swapchain images\n";
    return false;
  }

  if (swapchainImageCount == 0u)
  {
    std::cerr << "Found zero mirror view swapchain images\n";
    return false;
  }

  swapchainImages.resize(swapchainImageCount);
  if (vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()) != VK_SUCCESS)
  {
    std::cerr << "Failed to retrieve mirror view swapchain images\n";
    return false;
  }

  // Create new swapchain render targets
  swapchainRenderTargets.resize(swapchainImages.size());
  for (size_t i = 0u; i < swapchainRenderTargets.size(); ++i)
  {
    RenderTarget*& renderTarget = swapchainRenderTargets.at(i);

    VkImage image = swapchainImages.at(i);
    renderTarget = new RenderTarget(device, image, surfaceCapabilities.currentExtent, surfaceFormat.format, renderPass);
    if (!renderTarget->isValid())
    {
      return false;
    }
  }

  return true;
}

void MirrorView::destroySwapchain()
{
  for (RenderTarget* renderTarget : swapchainRenderTargets)
  {
    renderTarget->destroy();
    delete renderTarget;
    renderTarget = nullptr;
  }

  vkDestroySwapchainKHR(headset->getDevice(), swapchain, nullptr);
}