#include "RenderCommand.h"

#include "Util.h"

RenderCommand::RenderCommand(VkDevice device, VkCommandPool commandPool) : device(device)
{
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

  VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Make sure the fence starts off signaled
  if (vkCreateFence(device, &fenceCreateInfo, nullptr, &busyFence) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }
}

RenderCommand::~RenderCommand()
{
  vkDestroyFence(device, busyFence, nullptr);
  vkDestroySemaphore(device, presentableSemaphore, nullptr);
  vkDestroySemaphore(device, drawableSemaphore, nullptr);
}

bool RenderCommand::isValid() const
{
  return valid;
}

VkCommandBuffer RenderCommand::getCommandBuffer() const
{
  return commandBuffer;
}

VkSemaphore RenderCommand::getDrawableSemaphore() const
{
  return drawableSemaphore;
}

VkSemaphore RenderCommand::getPresentableSemaphore() const
{
  return presentableSemaphore;
}

VkFence RenderCommand::getBusyFence() const
{
  return busyFence;
}