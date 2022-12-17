#pragma once

#include <vulkan/vulkan.h>

/*
 * The buffer class is used to store Vulkan data buffers, namely the uniform buffer and the vertex/index buffer. It is
 * unrelated to Vulkan image buffers used for the depth buffer for example. Note that is good for performance to keep
 * Vulkan buffers mapped until destruction. This class offers functionality to do so, but doesn't enfore the principle.
 */
class Buffer final
{
public:
  Buffer(VkDevice device,
         VkPhysicalDevice physicalDevice,
         VkBufferUsageFlags bufferUsageFlags,
         VkMemoryPropertyFlags memoryProperties,
         VkDeviceSize size);
  ~Buffer();

  bool copyTo(const Buffer& target, VkCommandBuffer commandBuffer, VkQueue queue) const;
  void* map() const;
  void unmap() const;

  bool isValid() const;
  VkBuffer getVkBuffer() const;

private:
  bool valid = true;

  VkDevice device = nullptr;
  VkBuffer buffer = nullptr;
  VkDeviceMemory deviceMemory = nullptr;
  VkDeviceSize size = 0u;
};