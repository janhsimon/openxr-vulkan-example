#pragma once

#include "Context.h"

/*
 * The image buffer class represents a convienent combination of an image, its associated memory, and a corresponding
 * image view in Vulkan. The class is used to bundle all required resources for the color and depth buffer respectively.
 */
class ImageBuffer final
{
public:
  ImageBuffer(const Context* context,
              VkExtent2D size,
              VkFormat format,
              VkImageUsageFlags usage,
              VkSampleCountFlagBits samples,
              VkImageAspectFlags aspect,
              size_t layerCount);
  ~ImageBuffer();

  bool isValid() const;

  VkImageView getImageView() const;

private:
  bool valid = true;

  const Context* context = nullptr;
  VkImage image = nullptr;
  VkDeviceMemory deviceMemory = nullptr;
  VkImageView imageView = nullptr;
};