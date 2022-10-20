#pragma once

#include <vulkan/vulkan.h>

class Buffer;
class Headset;
class Pipeline;

class Renderer final
{
public:
  Renderer(const Headset* headset);

  void destroy() const; // Only call when construction succeeded

  void render(size_t swapchainImageIndex) const;

  bool isValid() const;

private:
  bool valid = true;

  const Headset* headset = nullptr;
  VkDescriptorSetLayout descriptorSetLayout = nullptr;
  VkDescriptorPool descriptorPool = nullptr;
  VkDescriptorSet descriptorSet = nullptr;
  VkPipelineLayout pipelineLayout = nullptr;
  VkCommandPool commandPool = nullptr;
  VkCommandBuffer commandBuffer = nullptr;
  VkFence fence = nullptr;
  Buffer *uniformBuffer = nullptr, *vertexBuffer = nullptr, *indexBuffer = nullptr;
  Pipeline *gridPipeline = nullptr, *cubePipeline = nullptr;
};