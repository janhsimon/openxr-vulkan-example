#pragma once

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

class Buffer;

class RenderProcess final
{
public:
  RenderProcess(VkDevice device,
                VkPhysicalDevice physicalDevice,
                VkCommandPool commandPool,
                VkDescriptorPool descriptorPool,
                VkDescriptorSetLayout descriptorSetLayout);
  ~RenderProcess();

  struct UniformBufferData final
  {
    glm::mat4 world;
    glm::mat4 viewProjection[2]; // View projection matrices, 0 = left eye, 1 = right eye
    glm::mat4 padding;           // Pad to 256 bytes
    float time;                  // For animation
  } uniformBufferData;

  bool isValid() const;
  VkCommandBuffer getCommandBuffer() const;
  VkSemaphore getDrawableSemaphore() const;
  VkSemaphore getPresentableSemaphore() const;
  VkFence getBusyFence() const;
  VkDescriptorSet getDescriptorSet() const;

  void updateUniformBufferData() const;

private:
  bool valid = true;

  VkDevice device = nullptr;
  VkCommandBuffer commandBuffer = nullptr;
  VkSemaphore drawableSemaphore = nullptr, presentableSemaphore = nullptr;
  VkFence busyFence = nullptr;
  Buffer* uniformBuffer = nullptr;
  void* uniformBufferMemory = nullptr;
  VkDescriptorSet descriptorSet = nullptr;
};