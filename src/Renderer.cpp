#include "Renderer.h"

#include "Buffer.h"
#include "Context.h"
#include "Headset.h"
#include "Pipeline.h"
#include "RenderTarget.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>

namespace
{
struct Vertex final
{
  glm::vec3 position;
  glm::vec3 color;
};

constexpr std::array vertices = {
  // Grid
  Vertex({ -20.0f, 0.0f, -20.0f }, { 1.0f, 0.0f, 0.0f }), Vertex({ -20.0f, 0.0f, +20.0f }, { 0.0f, 1.0f, 0.0f }),
  Vertex({ +20.0f, 0.0f, -20.0f }, { 0.0f, 0.0f, 1.0f }), Vertex({ +20.0f, 0.0f, +20.0f }, { 1.0f, 0.0f, 1.0f }),

  // Cube front left
  Vertex({ -1.0f, 0.0f, -3.0f }, { 0.8f, 0.8f, 0.8f }), Vertex({ -1.0f, 1.0f, -3.0f }, { 0.8f, 0.8f, 0.8f }),
  Vertex({ +0.0f, 0.0f, -2.0f }, { 0.8f, 0.8f, 0.8f }), Vertex({ +0.0f, 1.0f, -2.0f }, { 0.8f, 0.8f, 0.8f }),

  // Cube front right
  Vertex({ +0.0f, 0.0f, -2.0f }, { 0.6f, 0.6f, 0.6f }), Vertex({ +0.0f, 1.0f, -2.0f }, { 0.6f, 0.6f, 0.8f }),
  Vertex({ +1.0f, 0.0f, -3.0f }, { 0.6f, 0.6f, 0.6f }), Vertex({ +1.0f, 1.0f, -3.0f }, { 0.6f, 0.6f, 0.8f }),

  // Cube back left
  Vertex({ -1.0f, 0.0f, -3.0f }, { 0.4f, 0.4f, 0.4f }), Vertex({ -1.0f, 1.0f, -3.0f }, { 0.4f, 0.4f, 0.4f }),
  Vertex({ +0.0f, 0.0f, -4.0f }, { 0.4f, 0.4f, 0.4f }), Vertex({ +0.0f, 1.0f, -4.0f }, { 0.4f, 0.4f, 0.4f }),

  // Cube back right
  Vertex({ +0.0f, 0.0f, -4.0f }, { 0.2f, 0.2f, 0.2f }), Vertex({ +0.0f, 1.0f, -4.0f }, { 0.2f, 0.2f, 0.2f }),
  Vertex({ +1.0f, 0.0f, -3.0f }, { 0.2f, 0.2f, 0.2f }), Vertex({ +1.0f, 1.0f, -3.0f }, { 0.2f, 0.2f, 0.2f }),

  // Cube top
  Vertex({ -1.0f, 1.0f, -3.0f }, { 1.0f, 1.0f, 1.0f }), Vertex({ +0.0f, 1.0f, -4.0f }, { 1.0f, 1.0f, 1.0f }),
  Vertex({ +1.0f, 1.0f, -3.0f }, { 1.0f, 1.0f, 1.0f }), Vertex({ +0.0f, 1.0f, -2.0f }, { 1.0f, 1.0f, 1.0f })
};

constexpr std::array<uint16_t, 36u> indices = { 0u,  1u,  2u,  1u,  2u,  3u,  4u,  5u,  6u,  5u,  6u,  7u,
                                                8u,  9u,  10u, 9u,  10u, 11u, 12u, 13u, 14u, 13u, 14u, 15u,
                                                16u, 17u, 18u, 17u, 18u, 19u, 20u, 21u, 22u, 20u, 22u, 23u };

struct UniformBufferObject final
{
  glm::mat4 world;
  glm::mat4 viewProjection[2];
} ubo;
} // namespace

Renderer::Renderer(const Context* context, const Headset* headset) : context(context), headset(headset)
{
  const VkPhysicalDevice vkPhysicalDevice = context->getVkPhysicalDevice();
  const VkDevice vkDevice = context->getVkDevice();

  // Create a command pool
  VkCommandPoolCreateInfo commandPoolCreateInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolCreateInfo.queueFamilyIndex = context->getVkDrawQueueFamilyIndex();
  if (vkCreateCommandPool(vkDevice, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Allocate a command buffer
  VkCommandBufferAllocateInfo commandBufferAllocateInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  commandBufferAllocateInfo.commandPool = commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = 1u;
  if (vkAllocateCommandBuffers(vkDevice, &commandBufferAllocateInfo, &commandBuffer) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Create the semaphores
  VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
  if (vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  if (vkCreateSemaphore(vkDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Create a memory fence
  VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  if (vkCreateFence(vkDevice, &fenceCreateInfo, nullptr, &inFlightFence) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Create an empty uniform buffer
  uniformBuffer = new Buffer(vkDevice, vkPhysicalDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             static_cast<VkDeviceSize>(sizeof(UniformBufferObject)));

  // Create a descriptor set layout
  VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
  descriptorSetLayoutBinding.binding = 0u;
  descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorSetLayoutBinding.descriptorCount = 1u;
  descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  descriptorSetLayoutCreateInfo.bindingCount = 1u;
  descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;
  if (vkCreateDescriptorSetLayout(vkDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) !=
      VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Create a descriptor pool
  VkDescriptorPoolSize descriptorPoolSize;
  descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptorPoolSize.descriptorCount = 1u;

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
  descriptorPoolCreateInfo.poolSizeCount = 1u;
  descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
  descriptorPoolCreateInfo.maxSets = 1u;
  if (vkCreateDescriptorPool(vkDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Allocate a descriptor set
  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  descriptorSetAllocateInfo.descriptorPool = descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = 1u;
  descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
  if (vkAllocateDescriptorSets(vkDevice, &descriptorSetAllocateInfo, &descriptorSet) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  VkDescriptorBufferInfo descriptorBufferInfo;
  descriptorBufferInfo.buffer = uniformBuffer->getBuffer();
  descriptorBufferInfo.offset = 0u;
  descriptorBufferInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
  writeDescriptorSet.dstSet = descriptorSet;
  writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSet.descriptorCount = 1u;
  writeDescriptorSet.dstBinding = 0u;
  writeDescriptorSet.dstArrayElement = 0u;
  vkUpdateDescriptorSets(vkDevice, 1u, &writeDescriptorSet, 0u, nullptr);

  // Create a pipeline layout
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
  pipelineLayoutCreateInfo.setLayoutCount = 1u;
  if (vkCreatePipelineLayout(vkDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
  {
    valid = false;
    return;
  }

  // Create the grid pipeline
  VkVertexInputBindingDescription vertexInputBindingDescription;
  vertexInputBindingDescription.binding = 0u;
  vertexInputBindingDescription.stride = sizeof(Vertex);
  vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexInputAttributeDescriptionPosition;
  vertexInputAttributeDescriptionPosition.binding = 0u;
  vertexInputAttributeDescriptionPosition.location = 0u;
  vertexInputAttributeDescriptionPosition.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributeDescriptionPosition.offset = offsetof(Vertex, position);

  VkVertexInputAttributeDescription vertexInputAttributeDescriptionColor;
  vertexInputAttributeDescriptionColor.binding = 0u;
  vertexInputAttributeDescriptionColor.location = 1u;
  vertexInputAttributeDescriptionColor.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributeDescriptionColor.offset = offsetof(Vertex, color);

  gridPipeline = new Pipeline(vkDevice, pipelineLayout, headset->getRenderPass(), "shaders/Basic.vert.spv",
                              "shaders/Grid.frag.spv", { vertexInputBindingDescription },
                              { vertexInputAttributeDescriptionPosition, vertexInputAttributeDescriptionColor });
  if (!gridPipeline->isValid())
  {
    valid = false;
    return;
  }

  // Create the cube pipeline
  cubePipeline = new Pipeline(vkDevice, pipelineLayout, headset->getRenderPass(), "shaders/Basic.vert.spv",
                              "shaders/Cube.frag.spv", { vertexInputBindingDescription },
                              { vertexInputAttributeDescriptionPosition, vertexInputAttributeDescriptionColor });
  if (!cubePipeline->isValid())
  {
    valid = false;
    return;
  }

  // Create a vertex buffer
  {
    // Create a staging buffer and fill it with the vertex data
    constexpr size_t size = sizeof(vertices);
    Buffer* stagingBuffer = new Buffer(vkDevice, vkPhysicalDevice, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       static_cast<VkDeviceSize>(size), static_cast<const void*>(vertices.data()));
    if (!stagingBuffer->isValid())
    {
      valid = false;
      return;
    }

    // Create an empty target buffer
    vertexBuffer =
      new Buffer(vkDevice, vkPhysicalDevice, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, static_cast<VkDeviceSize>(size));
    if (!vertexBuffer->isValid())
    {
      valid = false;
      return;
    }

    // Copy from the staging to the target buffer
    if (!stagingBuffer->copyTo(*vertexBuffer, commandBuffer, context->getVkDrawQueue()))
    {
      valid = false;
      return;
    }

    // Clean up the staging buffer
    stagingBuffer->destroy();
    delete stagingBuffer;
  }

  // Create an index buffer
  {
    // Create a staging buffer and fill it with the index data
    constexpr size_t size = sizeof(indices);
    Buffer* stagingBuffer = new Buffer(vkDevice, vkPhysicalDevice, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       static_cast<VkDeviceSize>(size), static_cast<const void*>(indices.data()));
    if (!stagingBuffer->isValid())
    {
      valid = false;
      return;
    }

    // Create an empty target buffer
    indexBuffer =
      new Buffer(vkDevice, vkPhysicalDevice, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, static_cast<VkDeviceSize>(size));
    if (!indexBuffer->isValid())
    {
      valid = false;
      return;
    }

    // Copy from the staging to the target buffer
    if (!stagingBuffer->copyTo(*indexBuffer, commandBuffer, context->getVkDrawQueue()))
    {
      valid = false;
      return;
    }

    // Clean up the staging buffer
    stagingBuffer->destroy();
    delete stagingBuffer;
  }
}

void Renderer::destroy() const
{
  indexBuffer->destroy();
  delete indexBuffer;

  vertexBuffer->destroy();
  delete vertexBuffer;

  cubePipeline->destroy();
  delete cubePipeline;

  gridPipeline->destroy();
  delete gridPipeline;

  const VkDevice vkDevice = context->getVkDevice();

  vkDestroyPipelineLayout(vkDevice, pipelineLayout, nullptr);
  vkDestroyDescriptorPool(vkDevice, descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(vkDevice, descriptorSetLayout, nullptr);

  uniformBuffer->destroy();
  delete uniformBuffer;

  vkDestroyFence(vkDevice, inFlightFence, nullptr);
  vkDestroySemaphore(vkDevice, renderFinishedSemaphore, nullptr);
  vkDestroySemaphore(vkDevice, imageAvailableSemaphore, nullptr);
  vkDestroyCommandPool(vkDevice, commandPool, nullptr);
}

void Renderer::render(size_t swapchainImageIndex) const
{
  if (vkResetFences(context->getVkDevice(), 1u, &inFlightFence) != VK_SUCCESS)
  {
    return;
  }

  if (vkResetCommandBuffer(commandBuffer, 0u) != VK_SUCCESS)
  {
    return;
  }

  VkCommandBufferBeginInfo commandBufferBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
  {
    return;
  }

  // Update the uniform buffer
  ubo.world = glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f });
  for (size_t eyeIndex = 0u; eyeIndex < headset->getEyeCount(); ++eyeIndex)
  {
    ubo.viewProjection[eyeIndex] = headset->getEyeProjectionMatrix(eyeIndex) * headset->getEyeViewMatrix(eyeIndex);
  }

  void* data = uniformBuffer->map();
  if (!data)
  {
    return;
  }

  memcpy(data, &ubo, sizeof(ubo));
  uniformBuffer->unmap();

  const std::array clearValues = { VkClearValue({ 0.5f, 0.5f, 0.5f, 1.0f }), VkClearValue({ 1.0f, 0u }) };

  VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
  renderPassBeginInfo.renderPass = headset->getRenderPass();
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

  // Bind the vertex buffer
  const VkDeviceSize offset = 0u;
  const VkBuffer buffer = vertexBuffer->getBuffer();
  vkCmdBindVertexBuffers(commandBuffer, 0u, 1u, &buffer, &offset);

  // Bind the index buffer
  vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0u, VK_INDEX_TYPE_UINT16);

  // Bind the uniform buffer
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0u, 1u, &descriptorSet, 0u,
                          nullptr);

  // Draw the grid
  gridPipeline->bind(commandBuffer);
  vkCmdDrawIndexed(commandBuffer, 6u, 1u, 0u, 0u, 0u);

  // Draw the cube
  cubePipeline->bind(commandBuffer);
  vkCmdDrawIndexed(commandBuffer, 30u, 1u, 6u, 0u, 0u);

  vkCmdEndRenderPass(commandBuffer);
}

void Renderer::submit() const
{
  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
  {
    return;
  }

  VkPipelineStageFlags waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

  VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
  submitInfo.waitSemaphoreCount = 1u;
  submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
  submitInfo.pWaitDstStageMask = &waitStages;
  submitInfo.commandBufferCount = 1u;
  submitInfo.pCommandBuffers = &commandBuffer;
  submitInfo.signalSemaphoreCount = 1u;
  submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
  if (vkQueueSubmit(context->getVkDrawQueue(), 1u, &submitInfo, inFlightFence) != VK_SUCCESS)
  {
    return;
  }
}

bool Renderer::isValid() const
{
  return valid;
}

VkCommandBuffer Renderer::getCommandBuffer() const
{
  return commandBuffer;
}

VkSemaphore Renderer::getImageAvailableSemaphore() const
{
  return imageAvailableSemaphore;
}

VkSemaphore Renderer::getRenderFinishedSemaphore() const
{
  return renderFinishedSemaphore;
}