#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class Buffer;
class Context;
class Headset;
class Pipeline;
class RenderCommand;

class Renderer final
{
public:
  Renderer(const Context* context, const Headset* headset);
  ~Renderer();

  void render(size_t swapchainImageIndex);
  void submit(bool useSemaphores) const;

  bool isValid() const;
  VkCommandBuffer getCurrentCommandBuffer() const;
  VkSemaphore getCurrentDrawableSemaphore() const;
  VkSemaphore getCurrentPresentableSemaphore() const;

private:
  bool valid = true;

  const Context* context = nullptr;
  const Headset* headset = nullptr;

  VkCommandPool commandPool = nullptr;
  size_t currentRenderCommand = 0u;
  std::vector<RenderCommand*> renderCommands;
  VkDescriptorSetLayout descriptorSetLayout = nullptr;
  VkDescriptorPool descriptorPool = nullptr;
  VkDescriptorSet descriptorSet = nullptr;
  VkPipelineLayout pipelineLayout = nullptr;

  Buffer *uniformBuffer = nullptr, *vertexBuffer = nullptr, *indexBuffer = nullptr;
  Pipeline *gridPipeline = nullptr, *cubePipeline = nullptr;
};