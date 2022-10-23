#pragma once

#include <vulkan/vulkan.h>

class Buffer;
class Context;
class Headset;
class Pipeline;

class Renderer final
{
public:
  Renderer(const Context* context, const Headset* headset);

  void destroy() const; // Only call when construction succeeded

  void render(size_t swapchainImageIndex) const;
  void submit() const;

  bool isValid() const;
  VkCommandBuffer getCommandBuffer() const;
  VkSemaphore getImageAvailableSemaphore() const;
  VkSemaphore getRenderFinishedSemaphore() const;

private:
  bool valid = true;

  const Context* context = nullptr;
  const Headset* headset = nullptr;

  VkCommandPool commandPool = nullptr;
  VkCommandBuffer commandBuffer = nullptr;
  VkSemaphore imageAvailableSemaphore = nullptr, renderFinishedSemaphore = nullptr;
  VkFence inFlightFence = nullptr;
  VkDescriptorSetLayout descriptorSetLayout = nullptr;
  VkDescriptorPool descriptorPool = nullptr;
  VkDescriptorSet descriptorSet = nullptr;
  VkPipelineLayout pipelineLayout = nullptr;

  Buffer *uniformBuffer = nullptr, *vertexBuffer = nullptr, *indexBuffer = nullptr;
  Pipeline *gridPipeline = nullptr, *cubePipeline = nullptr;
};