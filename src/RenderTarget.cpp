#include "RenderTarget.h"

RenderTarget::RenderTarget(VkDevice device,
                           VkImage image,
                           VkExtent2D size,
                           VkFormat format,
                           VkRenderPass renderPass,
                           uint32_t layerCount)
: device(device), image(image)
{
  // Create an image view
  VkImageViewCreateInfo imageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  imageViewCreateInfo.image = image;
  imageViewCreateInfo.format = format;
  imageViewCreateInfo.viewType = (layerCount == 1u ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
  imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  imageViewCreateInfo.subresourceRange.layerCount = layerCount;
  imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageViewCreateInfo.subresourceRange.baseArrayLayer = 0u;
  imageViewCreateInfo.subresourceRange.baseMipLevel = 0u;
  imageViewCreateInfo.subresourceRange.levelCount = 1u;
  if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Create a framebuffer
  VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
  framebufferCreateInfo.renderPass = renderPass;
  framebufferCreateInfo.attachmentCount = 1u;
  framebufferCreateInfo.pAttachments = &imageView;
  framebufferCreateInfo.width = size.width;
  framebufferCreateInfo.height = size.height;
  framebufferCreateInfo.layers = 1u;
  if (vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS)
  {
    valid = false;
    return;
  }
}

void RenderTarget::destroy() const
{
  vkDestroyFramebuffer(device, framebuffer, nullptr);
  vkDestroyImageView(device, imageView, nullptr);
}

bool RenderTarget::isValid() const
{
  return valid;
}

VkImage RenderTarget::getImage() const
{
  return image;
}

VkFramebuffer RenderTarget::getFramebuffer() const
{
  return framebuffer;
}
