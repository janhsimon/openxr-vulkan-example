#pragma once

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <vector>

class Buffer;

class RenderProcess final
{
public:
  RenderProcess(VkDevice device,
                VkPhysicalDevice physicalDevice,
                VkCommandPool commandPool,
                VkDescriptorPool descriptorPool,
                VkDescriptorSetLayout descriptorSetLayout,
                size_t numModels,
                VkDeviceSize uniformBufferOffsetAlignment);
  ~RenderProcess();

  struct DynamicVertexUniformData
  {
    glm::mat4 worldMatrix;
  };
  std::vector<DynamicVertexUniformData> dynamicVertexUniformData;

  struct StaticVertexUniformData
  {
    std::array<glm::mat4, 2u> viewProjectionMatrices; // 0 = left eye, 1 = right eye
  } staticVertexUniformData;

  struct StaticFragmentUniformData
  {
    float time;
  } staticFragmentUniformData;

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
  VkDeviceSize uniformBufferOffsetAlignment = 0u;
};