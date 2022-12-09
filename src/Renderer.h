#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class Buffer;
class Context;
class Headset;
class ModelLoader;
class Pipeline;
class RenderProcess;

class Renderer final
{
public:
  Renderer(const Context* context, const Headset* headset, const ModelLoader* modelLoader);
  ~Renderer();

  void render(size_t swapchainImageIndex, float deltaTime);
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
  VkDescriptorPool descriptorPool = nullptr;
  VkDescriptorSetLayout descriptorSetLayout = nullptr;
  std::vector<RenderProcess*> renderProcesses;
  VkPipelineLayout pipelineLayout = nullptr;
  Pipeline *gridPipeline = nullptr, *cubePipeline = nullptr;
  Buffer* geometryBuffer = nullptr;
  size_t geometryBufferIndexOffset = 0u;
  uint32_t numIndices = 0u, numGridIndices = 0u;
  size_t currentRenderProcessIndex = 0u;
};