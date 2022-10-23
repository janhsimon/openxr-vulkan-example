#include "Headset.h"

#include "Context.h"
#include "RenderTarget.h"
#include "Util.h"

#include <array>

namespace
{
inline constexpr XrReferenceSpaceType spaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
inline constexpr VkFormat colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
inline constexpr VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
} // namespace

Headset::Headset(const Context* context) : context(context)
{
  const VkPhysicalDevice vkPhysicalDevice = context->getVkPhysicalDevice();
  const VkDevice vkDevice = context->getVkDevice();
  const XrInstance xrInstance = context->getXrInstance();
  const XrSystemId xrSystemId = context->getXrSystemId();

  // Create a render pass
  {
    constexpr uint32_t viewMask = 0b00000011;
    constexpr uint32_t correlationMask = 0b00000011;

    VkRenderPassMultiviewCreateInfo renderPassMultiviewCreateInfo{
      VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO
    };
    renderPassMultiviewCreateInfo.subpassCount = 1u;
    renderPassMultiviewCreateInfo.pViewMasks = &viewMask;
    renderPassMultiviewCreateInfo.correlationMaskCount = 1u;
    renderPassMultiviewCreateInfo.pCorrelationMasks = &correlationMask;

    VkAttachmentDescription colorAttachmentDescription{};
    colorAttachmentDescription.format = colorFormat;
    colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0u;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachmentDescription{};
    depthAttachmentDescription.format = depthFormat;
    depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference;
    depthAttachmentReference.attachment = 1u;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1u;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    const std::array attachments = { colorAttachmentDescription, depthAttachmentDescription };

    VkRenderPassCreateInfo renderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassCreateInfo.pNext = &renderPassMultiviewCreateInfo;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassCreateInfo.pAttachments = attachments.data();
    renderPassCreateInfo.subpassCount = 1u;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    if (vkCreateRenderPass(vkDevice, &renderPassCreateInfo, nullptr, &vk.renderPass) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }

  const uint32_t vkDrawQueueFamilyIndex = context->getVkDrawQueueFamilyIndex();

  // Create a session with Vulkan graphics binding
  XrGraphicsBindingVulkanKHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR };
  graphicsBinding.device = vkDevice;
  graphicsBinding.instance = context->getVkInstance();
  graphicsBinding.physicalDevice = vkPhysicalDevice;
  graphicsBinding.queueFamilyIndex = vkDrawQueueFamilyIndex;
  graphicsBinding.queueIndex = 0u;

  XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
  sessionCreateInfo.next = &graphicsBinding;
  sessionCreateInfo.systemId = xrSystemId;
  XrResult result = xrCreateSession(xrInstance, &sessionCreateInfo, &xr.session);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Create a play space
  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
  referenceSpaceCreateInfo.referenceSpaceType = spaceType;
  referenceSpaceCreateInfo.poseInReferenceSpace = util::makeIdentity();
  result = xrCreateReferenceSpace(xr.session, &referenceSpaceCreateInfo, &xr.space);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  const XrViewConfigurationType viewType = context->getXrViewType();

  // Get the number of eyes
  result = xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, viewType, 0u,
                                             reinterpret_cast<uint32_t*>(&eyeCount), nullptr);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Get the eye image info per eye
  xr.eyeImageInfos.resize(eyeCount);
  for (XrViewConfigurationView& eyeInfo : xr.eyeImageInfos)
  {
    eyeInfo.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    eyeInfo.next = nullptr;
  }

  result =
    xrEnumerateViewConfigurationViews(xrInstance, xrSystemId, viewType, static_cast<uint32_t>(xr.eyeImageInfos.size()),
                                      reinterpret_cast<uint32_t*>(&eyeCount), xr.eyeImageInfos.data());
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Allocate the eye poses
  xr.eyePoses.resize(eyeCount);
  for (XrView& eyePose : xr.eyePoses)
  {
    eyePose.type = XR_TYPE_VIEW;
    eyePose.next = nullptr;
  }

  // Verify that the desired color format is supported
  {
    uint32_t formatCount = 0u;
    result = xrEnumerateSwapchainFormats(xr.session, 0u, &formatCount, nullptr);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    std::vector<int64_t> formats(formatCount);
    result = xrEnumerateSwapchainFormats(xr.session, formatCount, &formatCount, formats.data());
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    bool formatFound = false;
    for (const int64_t& format : formats)
    {
      if (format == static_cast<int64_t>(colorFormat))
      {
        formatFound = true;
        break;
      }
    }

    if (!formatFound)
    {
      error = Error::OpenXR;
      return;
    }
  }

  const VkExtent2D eyeResolution = getEyeResolution(0u);

  // Create a depth buffer
  {
    // Create an image
    VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = eyeResolution.width;
    imageCreateInfo.extent.height = eyeResolution.height;
    imageCreateInfo.extent.depth = 1u;
    imageCreateInfo.mipLevels = 1u;
    imageCreateInfo.arrayLayers = 2u;
    imageCreateInfo.format = depthFormat;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(vkDevice, &imageCreateInfo, nullptr, &vk.depthImage) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(vkDevice, vk.depthImage, &memoryRequirements);

    VkPhysicalDeviceMemoryProperties supportedMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &supportedMemoryProperties);

    const VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const VkMemoryPropertyFlags typeFilter = memoryRequirements.memoryTypeBits;
    uint32_t memoryTypeIndex = 0u;
    bool memoryTypeFound = false;
    for (uint32_t i = 0u; i < supportedMemoryProperties.memoryTypeCount; ++i)
    {
      const VkMemoryPropertyFlags propertyFlags = supportedMemoryProperties.memoryTypes[i].propertyFlags;
      if (typeFilter & (1 << i) && (propertyFlags & memoryProperties) == memoryProperties)
      {
        memoryTypeIndex = i;
        memoryTypeFound = true;
        break;
      }
    }

    if (!memoryTypeFound)
    {
      error = Error::Vulkan;
      return;
    }

    VkMemoryAllocateInfo memoryAllocateInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(vkDevice, &memoryAllocateInfo, nullptr, &vk.depthMemory) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }

    if (vkBindImageMemory(vkDevice, vk.depthImage, vk.depthMemory, 0) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }

    // Create an image view
    VkImageViewCreateInfo imageViewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    imageViewCreateInfo.image = vk.depthImage;
    imageViewCreateInfo.format = depthFormat;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                       VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    imageViewCreateInfo.subresourceRange.layerCount = 2u;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0u;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0u;
    imageViewCreateInfo.subresourceRange.levelCount = 1u;
    if (vkCreateImageView(vkDevice, &imageViewCreateInfo, nullptr, &vk.depthImageView) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }

  // Create a swapchain and render targets
  {
    const XrViewConfigurationView& eyeImageInfo = xr.eyeImageInfos.at(0u);

    // Create a swapchain
    XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
    swapchainCreateInfo.format = colorFormat;
    swapchainCreateInfo.sampleCount = eyeImageInfo.recommendedSwapchainSampleCount;
    swapchainCreateInfo.width = eyeImageInfo.recommendedImageRectWidth;
    swapchainCreateInfo.height = eyeImageInfo.recommendedImageRectHeight;
    swapchainCreateInfo.arraySize = static_cast<uint32_t>(eyeCount);
    swapchainCreateInfo.faceCount = 1u;
    swapchainCreateInfo.mipCount = 1u;

    result = xrCreateSwapchain(xr.session, &swapchainCreateInfo, &xr.swapchain);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    // Get the number of swapchain images
    uint32_t swapchainImageCount;
    result = xrEnumerateSwapchainImages(xr.swapchain, 0u, &swapchainImageCount, nullptr);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    // Retrieve the swapchain images
    std::vector<XrSwapchainImageVulkanKHR> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    for (XrSwapchainImageVulkanKHR& swapchainImage : swapchainImages)
    {
      swapchainImage.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
    }

    XrSwapchainImageBaseHeader* data = reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchainImages.data());
    result = xrEnumerateSwapchainImages(xr.swapchain, static_cast<uint32_t>(swapchainImages.size()),
                                        &swapchainImageCount, data);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    // Create the render targets
    xr.swapchainRenderTargets.resize(swapchainImages.size());
    for (size_t renderTargetIndex = 0u; renderTargetIndex < xr.swapchainRenderTargets.size(); ++renderTargetIndex)
    {
      RenderTarget*& renderTarget = xr.swapchainRenderTargets.at(renderTargetIndex);

      const VkImage image = swapchainImages.at(renderTargetIndex).image;
      renderTarget =
        new RenderTarget(vkDevice, image, vk.depthImageView, eyeResolution, colorFormat, vk.renderPass, 2u);
      if (!renderTarget->isValid())
      {
        error = Error::Vulkan;
        return;
      }
    }
  }

  // Create the eye render infos
  xr.eyeRenderInfos.resize(eyeCount);
  for (size_t eyeIndex = 0u; eyeIndex < xr.eyeRenderInfos.size(); ++eyeIndex)
  {
    XrCompositionLayerProjectionView& eyeRenderInfo = xr.eyeRenderInfos.at(eyeIndex);
    eyeRenderInfo.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    eyeRenderInfo.next = nullptr;

    // Associate this eye with the swapchain
    const XrViewConfigurationView& eyeImageInfo = xr.eyeImageInfos.at(eyeIndex);
    eyeRenderInfo.subImage.swapchain = xr.swapchain;
    eyeRenderInfo.subImage.imageArrayIndex = static_cast<uint32_t>(eyeIndex);
    eyeRenderInfo.subImage.imageRect.offset = { 0, 0 };
    eyeRenderInfo.subImage.imageRect.extent = { static_cast<int32_t>(eyeImageInfo.recommendedImageRectWidth),
                                                static_cast<int32_t>(eyeImageInfo.recommendedImageRectHeight) };
  }

  // Allocate view and projection matrices
  eyeViewMatrices.resize(eyeCount);
  eyeProjectionMatrices.resize(eyeCount);
}

void Headset::destroy() const
{
  // Clean up OpenXR
  xrEndSession(xr.session);
  xrDestroySwapchain(xr.swapchain);

  for (const RenderTarget* renderTarget : xr.swapchainRenderTargets)
  {
    renderTarget->destroy();
    delete renderTarget;
  }

  xrDestroySpace(xr.space);
  xrDestroySession(xr.session);

  // Clean up Vulkan
  const VkDevice vkDevice = context->getVkDevice();
  vkDestroyImageView(vkDevice, vk.depthImageView, nullptr);
  vkFreeMemory(vkDevice, vk.depthMemory, nullptr);
  vkDestroyImage(vkDevice, vk.depthImage, nullptr);
  vkDestroyRenderPass(vkDevice, vk.renderPass, nullptr);
}

Headset::BeginFrameResult Headset::beginFrame(uint32_t& swapchainImageIndex)
{
  const XrInstance instance = context->getXrInstance();

  // Poll OpenXR events
  XrEventDataBuffer buffer;
  buffer.type = XR_TYPE_EVENT_DATA_BUFFER;
  while (xrPollEvent(instance, &buffer) == XR_SUCCESS)
  {
    switch (buffer.type)
    {
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
      onSessionStateExiting();
      break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
    {
      XrEventDataSessionStateChanged* event = reinterpret_cast<XrEventDataSessionStateChanged*>(&buffer);
      xr.sessionState = event->state;

      if (event->state == XR_SESSION_STATE_READY)
      {
        if (!onSessionStateReady())
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_STOPPING)
      {
        if (!onSessionStateStopping())
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_LOSS_PENDING || event->state == XR_SESSION_STATE_EXITING)
      {
        if (!onSessionStateExiting())
        {
          return BeginFrameResult::Error;
        }
      }

      break;
    }
    }
  }

  if (xr.sessionState != XR_SESSION_STATE_READY && xr.sessionState != XR_SESSION_STATE_SYNCHRONIZED &&
      xr.sessionState != XR_SESSION_STATE_VISIBLE && xr.sessionState != XR_SESSION_STATE_FOCUSED)
  {
    // If we are not ready, synchronized, visible or focused, we skip all processing of this frame
    // This means no waiting, no beginning or ending of the frame at all
    return BeginFrameResult::SkipFully;
  }

  // Wait for the new frame
  xr.frameState.type = XR_TYPE_FRAME_STATE;
  XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
  XrResult result = xrWaitFrame(xr.session, &frameWaitInfo, &xr.frameState);
  if (XR_FAILED(result))
  {
    return BeginFrameResult::Error;
  }

  // Begin the new frame
  XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
  result = xrBeginFrame(xr.session, &frameBeginInfo);
  if (XR_FAILED(result))
  {
    return BeginFrameResult::Error;
  }

  if (!xr.frameState.shouldRender)
  {
    // Let the host know that we don't want to render this frame
    // We do still need to end the frame however
    return BeginFrameResult::SkipRender;
  }

  // Update the eye poses
  xr.viewState.type = XR_TYPE_VIEW_STATE;
  uint32_t viewCount;
  XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
  viewLocateInfo.viewConfigurationType = context->getXrViewType();
  viewLocateInfo.displayTime = xr.frameState.predictedDisplayTime;
  viewLocateInfo.space = xr.space;
  result = xrLocateViews(xr.session, &viewLocateInfo, &xr.viewState, static_cast<uint32_t>(xr.eyePoses.size()),
                         &viewCount, xr.eyePoses.data());
  if (XR_FAILED(result))
  {
    return BeginFrameResult::Error;
  }

  if (viewCount != eyeCount)
  {
    return BeginFrameResult::Error;
  }

  // Update the eye render infos, view and projection matrices
  for (size_t eyeIndex = 0u; eyeIndex < eyeCount; ++eyeIndex)
  {
    // Copy the eye poses into the eye render infos
    XrCompositionLayerProjectionView& eyeRenderInfo = xr.eyeRenderInfos.at(eyeIndex);
    const XrView& eyePose = xr.eyePoses.at(eyeIndex);
    eyeRenderInfo.pose = eyePose.pose;
    eyeRenderInfo.fov = eyePose.fov;

    // Update the view and projection matrices
    const XrPosef& pose = eyeRenderInfo.pose;
    eyeViewMatrices.at(eyeIndex) = util::poseToMatrix(pose);
    eyeProjectionMatrices.at(eyeIndex) = util::createProjectionMatrix(eyeRenderInfo.fov, 0.1f, 250.0f);
  }

  // Acquire the swapchain image
  XrSwapchainImageAcquireInfo swapchainImageAcquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
  result = xrAcquireSwapchainImage(xr.swapchain, &swapchainImageAcquireInfo, &swapchainImageIndex);
  if (XR_FAILED(result))
  {
    return BeginFrameResult::Error;
  }

  // Wait for the swapchain image
  XrSwapchainImageWaitInfo swapchainImageWaitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
  swapchainImageWaitInfo.timeout = XR_INFINITE_DURATION;
  result = xrWaitSwapchainImage(xr.swapchain, &swapchainImageWaitInfo);
  if (XR_FAILED(result))
  {
    return BeginFrameResult::Error;
  }

  return BeginFrameResult::RenderFully; // Request full rendering of the frame
}

void Headset::endFrame() const
{
  // Release the swapchain image
  XrSwapchainImageReleaseInfo swapchainImageReleaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
  XrResult result = xrReleaseSwapchainImage(xr.swapchain, &swapchainImageReleaseInfo);
  if (XR_FAILED(result))
  {
    return;
  }

  // End the frame
  XrCompositionLayerProjection compositionLayerProjection{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
  compositionLayerProjection.space = xr.space;
  compositionLayerProjection.viewCount = static_cast<uint32_t>(xr.eyeRenderInfos.size());
  compositionLayerProjection.views = xr.eyeRenderInfos.data();

  std::vector<XrCompositionLayerBaseHeader*> layers;

  const bool positionValid = xr.viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT;
  const bool orientationValid = xr.viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT;
  if (xr.frameState.shouldRender && positionValid && orientationValid)
  {
    layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&compositionLayerProjection));
  }

  XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
  frameEndInfo.displayTime = xr.frameState.predictedDisplayTime;
  frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
  frameEndInfo.layers = layers.data();
  frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  result = xrEndFrame(xr.session, &frameEndInfo);
  if (XR_FAILED(result))
  {
    return;
  }
}

Headset::Error Headset::getError() const
{
  return error;
}

VkRenderPass Headset::getRenderPass() const
{
  return vk.renderPass;
}

size_t Headset::getEyeCount() const
{
  return eyeCount;
}

VkExtent2D Headset::getEyeResolution(size_t eyeIndex) const
{
  const XrViewConfigurationView& eyeInfo = xr.eyeImageInfos.at(eyeIndex);
  return { eyeInfo.recommendedImageRectWidth, eyeInfo.recommendedImageRectHeight };
}

glm::mat4 Headset::getEyeViewMatrix(size_t eyeIndex) const
{
  return eyeViewMatrices.at(eyeIndex);
}

glm::mat4 Headset::getEyeProjectionMatrix(size_t eyeIndex) const
{
  return eyeProjectionMatrices.at(eyeIndex);
}

RenderTarget* Headset::getRenderTarget(size_t swapchainImageIndex) const
{
  return xr.swapchainRenderTargets.at(swapchainImageIndex);
}

bool Headset::onSessionStateReady() const
{
  // Start the session
  XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
  sessionBeginInfo.primaryViewConfigurationType = context->getXrViewType();
  const XrResult result = xrBeginSession(xr.session, &sessionBeginInfo);
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}

bool Headset::onSessionStateStopping() const
{
  // End the session
  const XrResult result = xrEndSession(xr.session);
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}

bool Headset::onSessionStateExiting() const
{
  // Destroy the session
  XrResult result = xrDestroySession(xr.session);
  if (XR_FAILED(result))
  {
    return false;
  }

  // Destroy the instance
  result = xrDestroyInstance(context->getXrInstance());
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}