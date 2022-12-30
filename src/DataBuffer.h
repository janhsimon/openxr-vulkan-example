#pragma once

#include <vulkan/vulkan.h>

class Context;

/*
 * The data buffer class is used to store Vulkan data buffers, namely the uniform buffer and the vertex/index buffer. It
 * is unrelated to Vulkan image buffers used for the depth buffer for example. Note that is good for performance to keep
 * Vulkan buffers mapped until destruction. This class offers functionality to do so, but doesn't enforce the principle.
 */
class DataBuffer final
{
public:
  DataBuffer(const Context* context,
             VkBufferUsageFlags bufferUsageFlags,
             VkMemoryPropertyFlags memoryProperties,
             VkDeviceSize size);
  ~DataBuffer();

  bool copyTo(const DataBuffer& target, VkCommandBuffer commandBuffer, VkQueue queue) const;
  void* map() const;
  void unmap() const;

  bool isValid() const;
  VkBuffer getBuffer() const;

private:
  bool valid = true;

  const Context* context = nullptr;
  VkBuffer buffer = nullptr;
  VkDeviceMemory deviceMemory = nullptr;
  VkDeviceSize size = 0u;
};