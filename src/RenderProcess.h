#pragma once

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <vector>

class Context;
class DataBuffer;

/*
 * The render process class consolidates all the resources that needs to be duplicated for each frame that can be
 * rendered to in parallel. The renderer owns a render process for each frame that can be processed at the same time,
 * and each render process holds their own uniform buffer, command buffer, semaphore and memory fence. With this
 * duplication, the application can be sure that one frame does not modify a resource that is still in use by another
 * simultaneous frame.
 */
class RenderProcess final
{
public:
  RenderProcess(const Context* context,
                VkCommandPool commandPool,
                VkDescriptorPool descriptorPool,
                VkDescriptorSetLayout descriptorSetLayout,
                size_t modelCount);
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

  const Context* context = nullptr;
  VkCommandBuffer commandBuffer = nullptr;
  VkSemaphore drawableSemaphore = nullptr, presentableSemaphore = nullptr;
  VkFence busyFence = nullptr;
  DataBuffer* uniformBuffer = nullptr;
  void* uniformBufferMemory = nullptr;
  VkDescriptorSet descriptorSet = nullptr;
};