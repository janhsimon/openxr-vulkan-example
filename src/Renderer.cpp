#include "Renderer.h"

#include "Context.h"
#include "DataBuffer.h"
#include "Headset.h"
#include "MeshData.h"
#include "Model.h"
#include "Pipeline.h"
#include "RenderProcess.h"
#include "RenderTarget.h"
#include "Util.h"

#include <array>

namespace
{
constexpr size_t framesInFlightCount = 2u;
} // namespace

Renderer::Renderer(const Context* context,
                   const Headset* headset,
                   const MeshData* meshData,
                   const std::vector<Model*>& models)
: context(context), headset(headset), models(models)
{
  const VkDevice device = context->getVkDevice();

  // Create a command pool
  VkCommandPoolCreateInfo commandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolCreateInfo.queueFamilyIndex = context->getVkDrawQueueFamilyIndex();
  if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  // Create a descriptor pool
  std::array<VkDescriptorPoolSize, 2u> descriptorPoolSizes;

  descriptorPoolSizes.at(0u).type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  descriptorPoolSizes.at(0u).descriptorCount = static_cast<uint32_t>(framesInFlightCount);

  descriptorPoolSizes.at(1u).type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorPoolSizes.at(1u).descriptorCount = static_cast<uint32_t>(framesInFlightCount * 2u);

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
  descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
  descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(framesInFlightCount);
  if (vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  // Create a descriptor set layout
  std::array<VkDescriptorSetLayoutBinding, 3u> descriptorSetLayoutBindings;

  descriptorSetLayoutBindings.at(0u).binding = 0u;
  descriptorSetLayoutBindings.at(0u).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  descriptorSetLayoutBindings.at(0u).descriptorCount = 1u;
  descriptorSetLayoutBindings.at(0u).stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  descriptorSetLayoutBindings.at(0u).pImmutableSamplers = nullptr;

  descriptorSetLayoutBindings.at(1u).binding = 1u;
  descriptorSetLayoutBindings.at(1u).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorSetLayoutBindings.at(1u).descriptorCount = 1u;
  descriptorSetLayoutBindings.at(1u).stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  descriptorSetLayoutBindings.at(1u).pImmutableSamplers = nullptr;

  descriptorSetLayoutBindings.at(2u).binding = 2u;
  descriptorSetLayoutBindings.at(2u).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorSetLayoutBindings.at(2u).descriptorCount = 1u;
  descriptorSetLayoutBindings.at(2u).stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  descriptorSetLayoutBindings.at(2u).pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
  descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
  if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  // Create a pipeline layout
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
  pipelineLayoutCreateInfo.setLayoutCount = 1u;
  if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  // Create a render process for each frame in flight
  renderProcesses.resize(framesInFlightCount);
  for (RenderProcess*& renderProcess : renderProcesses)
  {
    renderProcess = new RenderProcess(context, commandPool, descriptorPool, descriptorSetLayout, models.size());
    if (!renderProcess->isValid())
    {
      valid = false;
      return;
    }
  }

  // Create the grid pipeline
  VkVertexInputBindingDescription vertexInputBindingDescription;
  vertexInputBindingDescription.binding = 0u;
  vertexInputBindingDescription.stride = sizeof(Vertex);
  vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexInputAttributePosition;
  vertexInputAttributePosition.binding = 0u;
  vertexInputAttributePosition.location = 0u;
  vertexInputAttributePosition.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributePosition.offset = offsetof(Vertex, position);

  VkVertexInputAttributeDescription vertexInputAttributeNormal;
  vertexInputAttributeNormal.binding = 0u;
  vertexInputAttributeNormal.location = 1u;
  vertexInputAttributeNormal.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributeNormal.offset = offsetof(Vertex, normal);

  VkVertexInputAttributeDescription vertexInputAttributeColor;
  vertexInputAttributeColor.binding = 0u;
  vertexInputAttributeColor.location = 2u;
  vertexInputAttributeColor.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributeColor.offset = offsetof(Vertex, color);

  gridPipeline =
    new Pipeline(context, pipelineLayout, headset->getVkRenderPass(), "shaders/Grid.vert.spv", "shaders/Grid.frag.spv",
                 { vertexInputBindingDescription }, { vertexInputAttributePosition, vertexInputAttributeColor });
  if (!gridPipeline->isValid())
  {
    valid = false;
    return;
  }

  // Create the diffuse pipeline
  diffusePipeline =
    new Pipeline(context, pipelineLayout, headset->getVkRenderPass(), "shaders/Diffuse.vert.spv",
                 "shaders/Diffuse.frag.spv", { vertexInputBindingDescription },
                 { vertexInputAttributePosition, vertexInputAttributeNormal, vertexInputAttributeColor });
  if (!diffusePipeline->isValid())
  {
    valid = false;
    return;
  }

  // Create a vertex index buffer
  {
    // Create a staging buffer
    const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(meshData->getSize());
    DataBuffer* stagingBuffer =
      new DataBuffer(context, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bufferSize);
    if (!stagingBuffer->isValid())
    {
      valid = false;
      return;
    }

    // Fill the staging buffer with vertex and index data
    char* bufferData = static_cast<char*>(stagingBuffer->map());
    if (!bufferData)
    {
      valid = false;
      return;
    }

    meshData->writeTo(bufferData);
    stagingBuffer->unmap();

    // Create an empty target buffer
    vertexIndexBuffer = new DataBuffer(context,
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferSize);
    if (!vertexIndexBuffer->isValid())
    {
      valid = false;
      return;
    }

    // Copy from the staging to the target buffer
    if (!stagingBuffer->copyTo(*vertexIndexBuffer, renderProcesses.at(0u)->getCommandBuffer(),
                               context->getVkDrawQueue()))
    {
      valid = false;
      return;
    }

    // Clean up the staging buffer
    delete stagingBuffer;
  }

  indexOffset = meshData->getIndexOffset();
}

Renderer::~Renderer()
{
  delete vertexIndexBuffer;
  delete diffusePipeline;
  delete gridPipeline;

  const VkDevice device = context->getVkDevice();
  if (device)
  {
    if (pipelineLayout)
    {
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }

    if (descriptorSetLayout)
    {
      vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    }

    if (descriptorPool)
    {
      vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
  }

  for (const RenderProcess* renderProcess : renderProcesses)
  {
    delete renderProcess;
  }

  if (device && commandPool)
  {
    vkDestroyCommandPool(device, commandPool, nullptr);
  }
}

void Renderer::render(const glm::mat4& cameraMatrix, size_t swapchainImageIndex, float time)
{
  currentRenderProcessIndex = (currentRenderProcessIndex + 1u) % renderProcesses.size();

  RenderProcess* renderProcess = renderProcesses.at(currentRenderProcessIndex);

  const VkFence busyFence = renderProcess->getBusyFence();
  if (vkResetFences(context->getVkDevice(), 1u, &busyFence) != VK_SUCCESS)
  {
    return;
  }

  const VkCommandBuffer commandBuffer = renderProcess->getCommandBuffer();

  if (vkResetCommandBuffer(commandBuffer, 0u) != VK_SUCCESS)
  {
    return;
  }

  VkCommandBufferBeginInfo commandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
  {
    return;
  }

  // Update the uniform buffer data
  {
    for (size_t modelIndex = 0u; modelIndex < models.size(); ++modelIndex)
    {
      renderProcess->dynamicVertexUniformData.at(modelIndex).worldMatrix = models.at(modelIndex)->worldMatrix;
    }

    for (size_t eyeIndex = 0u; eyeIndex < headset->getEyeCount(); ++eyeIndex)
    {
      renderProcess->staticVertexUniformData.viewProjectionMatrices.at(eyeIndex) =
        headset->getEyeProjectionMatrix(eyeIndex) * headset->getEyeViewMatrix(eyeIndex) * cameraMatrix;
    }

    renderProcess->staticFragmentUniformData.time = time;

    renderProcess->updateUniformBufferData();
  }

  const std::array clearValues = { VkClearValue({ 0.01f, 0.01f, 0.01f, 1.0f }), VkClearValue({ 1.0f, 0u }) };

  VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
  renderPassBeginInfo.renderPass = headset->getVkRenderPass();
  renderPassBeginInfo.framebuffer = headset->getRenderTarget(swapchainImageIndex)->getFramebuffer();
  renderPassBeginInfo.renderArea.offset = { 0, 0 };
  renderPassBeginInfo.renderArea.extent = headset->getEyeResolution(0u);
  renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassBeginInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  // Set the viewport
  VkViewport viewport;
  viewport.x = static_cast<float>(renderPassBeginInfo.renderArea.offset.x);
  viewport.y = static_cast<float>(renderPassBeginInfo.renderArea.offset.y);
  viewport.width = static_cast<float>(renderPassBeginInfo.renderArea.extent.width);
  viewport.height = static_cast<float>(renderPassBeginInfo.renderArea.extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0u, 1u, &viewport);

  // Set the scissor
  VkRect2D scissor;
  scissor.offset = renderPassBeginInfo.renderArea.offset;
  scissor.extent = renderPassBeginInfo.renderArea.extent;
  vkCmdSetScissor(commandBuffer, 0u, 1u, &scissor);

  // Bind the vertex section of the geometry buffer
  VkDeviceSize vertexOffset = 0u;
  const VkBuffer buffer = vertexIndexBuffer->getBuffer();
  vkCmdBindVertexBuffers(commandBuffer, 0u, 1u, &buffer, &vertexOffset);

  // Bind the index section of the geometry buffer
  vkCmdBindIndexBuffer(commandBuffer, buffer, indexOffset, VK_INDEX_TYPE_UINT32);

  // Draw each model
  const VkDescriptorSet descriptorSet = renderProcess->getDescriptorSet();
  for (size_t modelIndex = 0u; modelIndex < models.size(); ++modelIndex)
  {
    const Model* model = models.at(modelIndex);

    // Bind the uniform buffer
    const uint32_t uniformBufferOffset =
      static_cast<uint32_t>(util::align(static_cast<VkDeviceSize>(sizeof(RenderProcess::DynamicVertexUniformData)),
                                        context->getUniformBufferOffsetAlignment()) *
                            static_cast<VkDeviceSize>(modelIndex));
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0u, 1u, &descriptorSet, 1u,
                            &uniformBufferOffset);

    // Bind the pipeline
    if (modelIndex == 0u)
    {
      gridPipeline->bind(commandBuffer);
    }
    else if (modelIndex == 1u)
    {
      diffusePipeline->bind(commandBuffer);
    }

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(model->indexCount), 1u,
                     static_cast<uint32_t>(model->firstIndex), 0u, 0u);
  }

  vkCmdEndRenderPass(commandBuffer);
}

void Renderer::submit(bool useSemaphores) const
{
  const RenderProcess* renderProcess = renderProcesses.at(currentRenderProcessIndex);
  const VkCommandBuffer commandBuffer = renderProcess->getCommandBuffer();

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
  {
    return;
  }

  constexpr VkPipelineStageFlags waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  const VkSemaphore drawableSemaphore = renderProcess->getDrawableSemaphore();
  const VkSemaphore presentableSemaphore = renderProcess->getPresentableSemaphore();

  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.pWaitDstStageMask = &waitStages;
  submitInfo.commandBufferCount = 1u;
  submitInfo.pCommandBuffers = &commandBuffer;

  if (useSemaphores)
  {
    submitInfo.waitSemaphoreCount = 1u;
    submitInfo.pWaitSemaphores = &drawableSemaphore;
    submitInfo.signalSemaphoreCount = 1u;
    submitInfo.pSignalSemaphores = &presentableSemaphore;
  }

  const VkFence busyFence = renderProcess->getBusyFence();
  if (vkQueueSubmit(context->getVkDrawQueue(), 1u, &submitInfo, busyFence) != VK_SUCCESS)
  {
    return;
  }
}

bool Renderer::isValid() const
{
  return valid;
}

VkCommandBuffer Renderer::getCurrentCommandBuffer() const
{
  return renderProcesses.at(currentRenderProcessIndex)->getCommandBuffer();
}

VkSemaphore Renderer::getCurrentDrawableSemaphore() const
{
  return renderProcesses.at(currentRenderProcessIndex)->getDrawableSemaphore();
}

VkSemaphore Renderer::getCurrentPresentableSemaphore() const
{
  return renderProcesses.at(currentRenderProcessIndex)->getPresentableSemaphore();
}