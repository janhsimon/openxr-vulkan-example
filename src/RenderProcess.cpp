#include "RenderProcess.h"

#include "Context.h"
#include "DataBuffer.h"
#include "Util.h"

#include <cstring>

RenderProcess::RenderProcess(const Context* context,
                             VkCommandPool commandPool,
                             VkDescriptorPool descriptorPool,
                             VkDescriptorSetLayout descriptorSetLayout,
                             size_t modelCount)
: context(context)
{
  // Initialize the uniform buffer data
  dynamicVertexUniformData.resize(modelCount);
  for (size_t modelIndex = 0u; modelIndex < modelCount; ++modelIndex)
  {
    dynamicVertexUniformData.at(modelIndex).worldMatrix = glm::mat4(1.0f);
  }

  for (glm::mat4& viewProjectionMatrix : staticVertexUniformData.viewProjectionMatrices)
  {
    viewProjectionMatrix = glm::mat4(1.0f);
  }

  staticFragmentUniformData.time = 0.0f;

  const VkDevice device = context->getVkDevice();

  // Allocate a command buffer
  VkCommandBufferAllocateInfo commandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  commandBufferAllocateInfo.commandPool = commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = 1u;
  if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

#ifdef DEBUG
  if (context->setDebugObjectName(reinterpret_cast<uint64_t>(commandBuffer), VK_OBJECT_TYPE_COMMAND_BUFFER,
                                  "OXR_VK_X Command Buffer") != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }
#endif

  // Create semaphores
  VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &drawableSemaphore) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentableSemaphore) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  // Create a fence
  VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Make sure the fence starts off signaled
  if (vkCreateFence(device, &fenceCreateInfo, nullptr, &busyFence) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  const VkDeviceSize uniformBufferOffsetAlignment = context->getUniformBufferOffsetAlignment();

  // Partition the uniform buffer data
  std::array<VkDescriptorBufferInfo, 3u> descriptorBufferInfos;

  descriptorBufferInfos.at(0u).offset = 0u;
  descriptorBufferInfos.at(0u).range = sizeof(DynamicVertexUniformData);

  descriptorBufferInfos.at(1u).offset = util::align(descriptorBufferInfos.at(0u).range, uniformBufferOffsetAlignment) *
                                        static_cast<VkDeviceSize>(modelCount);
  descriptorBufferInfos.at(1u).range = sizeof(StaticVertexUniformData);

  descriptorBufferInfos.at(2u).offset =
    descriptorBufferInfos.at(1u).offset + util::align(descriptorBufferInfos.at(1u).range, uniformBufferOffsetAlignment);
  descriptorBufferInfos.at(2u).range = sizeof(StaticFragmentUniformData);

  // Create an empty uniform buffer
  const VkDeviceSize uniformBufferSize = descriptorBufferInfos.at(2u).offset + descriptorBufferInfos.at(2u).range;
  uniformBuffer =
    new DataBuffer(context, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBufferSize);
  if (!uniformBuffer->isValid())
  {
    valid = false;
    return;
  }

  // Map the uniform buffer memory
  uniformBufferMemory = uniformBuffer->map();
  if (!uniformBufferMemory)
  {
    valid = false;
    return;
  }

  // Allocate a descriptor set
  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  descriptorSetAllocateInfo.descriptorPool = descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = 1u;
  descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
  const VkResult result = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
  if (result != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  // Associate the uniform buffer with each descriptor buffer info
  for (VkDescriptorBufferInfo& descriptorBufferInfo : descriptorBufferInfos)
  {
    descriptorBufferInfo.buffer = uniformBuffer->getBuffer();
  }

  // Update the descriptor sets
  std::array<VkWriteDescriptorSet, 3u> writeDescriptorSets;

  writeDescriptorSets.at(0u).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSets.at(0u).pNext = nullptr;
  writeDescriptorSets.at(0u).dstSet = descriptorSet;
  writeDescriptorSets.at(0u).dstBinding = 0u;
  writeDescriptorSets.at(0u).dstArrayElement = 0u;
  writeDescriptorSets.at(0u).descriptorCount = 1u;
  writeDescriptorSets.at(0u).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  writeDescriptorSets.at(0u).pBufferInfo = &descriptorBufferInfos.at(0u);
  writeDescriptorSets.at(0u).pImageInfo = nullptr;
  writeDescriptorSets.at(0u).pTexelBufferView = nullptr;

  writeDescriptorSets.at(1u).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSets.at(1u).pNext = nullptr;
  writeDescriptorSets.at(1u).dstSet = descriptorSet;
  writeDescriptorSets.at(1u).dstBinding = 1u;
  writeDescriptorSets.at(1u).dstArrayElement = 0u;
  writeDescriptorSets.at(1u).descriptorCount = 1u;
  writeDescriptorSets.at(1u).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSets.at(1u).pBufferInfo = &descriptorBufferInfos.at(1u);
  writeDescriptorSets.at(1u).pImageInfo = nullptr;
  writeDescriptorSets.at(1u).pTexelBufferView = nullptr;

  writeDescriptorSets.at(2u).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSets.at(2u).pNext = nullptr;
  writeDescriptorSets.at(2u).dstSet = descriptorSet;
  writeDescriptorSets.at(2u).dstBinding = 2u;
  writeDescriptorSets.at(2u).dstArrayElement = 0u;
  writeDescriptorSets.at(2u).descriptorCount = 1u;
  writeDescriptorSets.at(2u).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSets.at(2u).pBufferInfo = &descriptorBufferInfos.at(2u);
  writeDescriptorSets.at(2u).pImageInfo = nullptr;
  writeDescriptorSets.at(2u).pTexelBufferView = nullptr;

  vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0u,
                         nullptr);
}

RenderProcess::~RenderProcess()
{
  if (uniformBuffer)
  {
    uniformBuffer->unmap();
  }
  delete uniformBuffer;

  const VkDevice device = context->getVkDevice();
  if (device)
  {
    if (busyFence)
    {
      vkDestroyFence(device, busyFence, nullptr);
    }

    if (presentableSemaphore)
    {
      vkDestroySemaphore(device, presentableSemaphore, nullptr);
    }

    if (drawableSemaphore)
    {
      vkDestroySemaphore(device, drawableSemaphore, nullptr);
    }
  }
}

bool RenderProcess::isValid() const
{
  return valid;
}

VkCommandBuffer RenderProcess::getCommandBuffer() const
{
  return commandBuffer;
}

VkSemaphore RenderProcess::getDrawableSemaphore() const
{
  return drawableSemaphore;
}

VkSemaphore RenderProcess::getPresentableSemaphore() const
{
  return presentableSemaphore;
}

VkFence RenderProcess::getBusyFence() const
{
  return busyFence;
}

VkDescriptorSet RenderProcess::getDescriptorSet() const
{
  return descriptorSet;
}

void RenderProcess::updateUniformBufferData() const
{
  if (!uniformBufferMemory)
  {
    return;
  }

  const VkDeviceSize uniformBufferOffsetAlignment = context->getUniformBufferOffsetAlignment();

  char* offset = static_cast<char*>(uniformBufferMemory);
  VkDeviceSize length = sizeof(DynamicVertexUniformData);
  for (const DynamicVertexUniformData& dynamicData : dynamicVertexUniformData)
  {
    memcpy(offset, &dynamicData, length);
    offset += util::align(length, uniformBufferOffsetAlignment);
  }

  length = sizeof(StaticVertexUniformData);
  memcpy(offset, &staticVertexUniformData, length);
  offset += util::align(length, uniformBufferOffsetAlignment);

  length = sizeof(StaticFragmentUniformData);
  memcpy(offset, &staticFragmentUniformData, length);
}