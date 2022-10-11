#include "Buffer.h"

#include <iostream>

Buffer::Buffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags)
: device(device), size(size)
{
  VkBufferCreateInfo bufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = bufferUsageFlags;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS)
  {
    std::cerr << "Failed to create buffer of " << size << " bytes\n";
    valid = false;
    return;
  }

  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

  const uint32_t properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  const VkMemoryPropertyFlags typeFilter = memoryRequirements.memoryTypeBits;
  uint32_t memoryTypeIndex = 0u;
  bool memoryTypeFound = false;
  for (uint32_t i = 0u; i < memoryProperties.memoryTypeCount; ++i)
  {
    const VkMemoryPropertyFlags propertyFlags = memoryProperties.memoryTypes[i].propertyFlags;
    if (typeFilter & (1 << i) && (propertyFlags & properties) == properties)
    {
      memoryTypeIndex = i;
      memoryTypeFound = true;
      break;
    }
  }

  if (!memoryTypeFound)
  {
    std::cerr << "Failed to find suitable memory type for buffer\n";
    valid = false;
    return;
  }

  VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
  if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS)
  {
    std::cerr << "Failed to allocate " << memoryRequirements.size << " bytes for buffer\n";
    valid = false;
    return;
  }

  if (vkBindBufferMemory(device, buffer, deviceMemory, 0u) != VK_SUCCESS)
  {
    std::cerr << "Failed to bind memory to buffer\n";
    valid = false;
    return;
  }
}

void Buffer::destroy()
{
  if (!valid)
  {
    return;
  }
  valid = false;

  vkFreeMemory(device, deviceMemory, nullptr);
  vkDestroyBuffer(device, buffer, nullptr);
}

void* Buffer::map() const
{
  void* data;
  if (vkMapMemory(device, deviceMemory, 0u, size, 0, &data) != VK_SUCCESS)
  {
    std::cerr << "Failed to map memory of buffer\n";
    return nullptr;
  }

  return data;
}

void Buffer::unmap() const
{
  vkUnmapMemory(device, deviceMemory);
}

bool Buffer::isValid() const
{
  return valid;
}

VkBuffer Buffer::getBuffer() const
{
  return buffer;
}