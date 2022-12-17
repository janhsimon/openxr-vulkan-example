#pragma once

#include <vulkan/vulkan.h>

/*
 * The render target class represents a convenient combination of an image and a framebuffer in Vulkan. The class is
 * used for the Vulkan swapchain images retrieved by OpenXR for the headset displays.
 */
class RenderTarget final
{
public:
  RenderTarget(VkDevice device,
               VkImage image,
               VkImageView depthImageView,
               VkExtent2D size,
               VkFormat format,
               VkRenderPass renderPass,
               uint32_t layerCount);
  ~RenderTarget();

  bool isValid() const;
  VkImage getImage() const;
  VkFramebuffer getFramebuffer() const;

private:
  bool valid = true;

  VkDevice device = nullptr;
  VkImage image = nullptr;
  VkImageView imageView = nullptr;
  VkFramebuffer framebuffer = nullptr;
};