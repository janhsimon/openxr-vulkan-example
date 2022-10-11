#pragma once

#include <vulkan/vulkan.h>

class Buffer final
{
public:
  Buffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags);

  void destroy();

  void* map() const;
  void unmap() const;

  bool isValid() const;
  VkBuffer getBuffer() const;

private:
  bool valid = true;

  VkDevice device = nullptr;
  VkBuffer buffer = nullptr;
  VkDeviceMemory deviceMemory = nullptr;
  VkDeviceSize size = 0u;
};