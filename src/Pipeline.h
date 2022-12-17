#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

/*
 * The pipeline class wraps a Vulkan pipeline for convenience. It describes the rendering technique to use, including
 * shaders, culling, scissoring, and other aspects.
 */
class Pipeline final
{
public:
  Pipeline(VkDevice device,
           VkPipelineLayout pipelineLayout,
           VkRenderPass renderPass,
           const std::string& vertexFilename,
           const std::string& fragmentFilename,
           const std::vector<VkVertexInputBindingDescription>& vertexInputBindingDescriptions,
           const std::vector<VkVertexInputAttributeDescription>& vertexInputAttributeDescriptions);
  ~Pipeline();

  void bind(VkCommandBuffer commandBuffer) const;

  bool isValid() const;

private:
  bool valid = true;

  VkDevice device = nullptr;
  VkPipeline pipeline = nullptr;
};