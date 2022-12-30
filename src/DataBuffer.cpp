#include "DataBuffer.h"

#include "Context.h"
#include "Util.h"

#include <sstream>

DataBuffer::DataBuffer(const Context* context,
                       const VkBufferUsageFlags bufferUsageFlags,
                       const VkMemoryPropertyFlags memoryProperties,
                       const VkDeviceSize size)
: context(context), size(size)
{
  const VkDevice device = context->getVkDevice();

  VkBufferCreateInfo bufferCreateInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferCreateInfo.size = size;
  bufferCreateInfo.usage = bufferUsageFlags;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  VkMemoryRequirements memoryRequirements;
  vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

  uint32_t suitableMemoryTypeIndex = 0u;
  if (!util::findSuitableMemoryTypeIndex(context->getVkPhysicalDevice(), memoryRequirements, memoryProperties,
                                         suitableMemoryTypeIndex))
  {
    util::error(Error::FeatureNotSupported, "Suitable data buffer memory type");
    valid = false;
  }

  VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex = suitableMemoryTypeIndex;
  if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS)
  {
    std::stringstream s;
    s << memoryRequirements.size << " bytes for buffer";
    util::error(Error::OutOfMemory, s.str());
    valid = false;
    return;
  }

  if (vkBindBufferMemory(device, buffer, deviceMemory, 0u) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }
}

DataBuffer::~DataBuffer()
{
  const VkDevice device = context->getVkDevice();
  if (device)
  {
    if (deviceMemory)
    {
      vkFreeMemory(device, deviceMemory, nullptr);
    }

    if (buffer)
    {
      vkDestroyBuffer(device, buffer, nullptr);
    }
  }
}

bool DataBuffer::copyTo(const DataBuffer& target, VkCommandBuffer commandBuffer, VkQueue queue) const
{
  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, buffer, target.getBuffer(), 1u, &copyRegion);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.commandBufferCount = 1u;
  submitInfo.pCommandBuffers = &commandBuffer;
  if (vkQueueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  if (vkQueueWaitIdle(queue) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return false;
  }

  return true;
}

void* DataBuffer::map() const
{
  void* data;
  const VkResult result = vkMapMemory(context->getVkDevice(), deviceMemory, 0u, size, 0, &data);
  if (result != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    return nullptr;
  }

  return data;
}

void DataBuffer::unmap() const
{
  vkUnmapMemory(context->getVkDevice(), deviceMemory);
}

bool DataBuffer::isValid() const
{
  return valid;
}

VkBuffer DataBuffer::getBuffer() const
{
  return buffer;
}