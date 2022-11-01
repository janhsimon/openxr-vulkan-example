#pragma once

#include <vulkan/vulkan.h>

class RenderCommand final
{
public:
  RenderCommand(VkDevice device, VkCommandPool commandPool);
  ~RenderCommand();

  bool isValid() const;
  VkCommandBuffer getCommandBuffer() const;
  VkSemaphore getDrawableSemaphore() const;
  VkSemaphore getPresentableSemaphore() const;
  VkFence getBusyFence() const;

private:
  bool valid = true;

  VkDevice device = nullptr;
  VkCommandBuffer commandBuffer = nullptr;
  VkSemaphore drawableSemaphore = nullptr, presentableSemaphore = nullptr;
  VkFence busyFence = nullptr;
};