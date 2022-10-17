#include "Headset.h"

#include "RenderTarget.h"

#include <glfw/glfw3.h>
#include <glm/gtx/quaternion.hpp>

#include <sstream>

#ifdef DEBUG
  #include <array>
  #include <iostream>
#endif

namespace
{
inline constexpr XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
inline constexpr XrEnvironmentBlendMode environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
inline constexpr XrReferenceSpaceType spaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
inline constexpr VkFormat colorFormat = VK_FORMAT_R8G8B8A8_SRGB;

bool loadXrExtensionFunction(XrInstance instance, const std::string& name, PFN_xrVoidFunction* function)
{
  const XrResult result = xrGetInstanceProcAddr(instance, name.c_str(), function);
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}

PFN_vkVoidFunction loadVkExtensionFunction(VkInstance instance, const std::string& name)
{
  return vkGetInstanceProcAddr(instance, name.c_str());
}

std::vector<const char*> extensionStringToVector(const std::string& string)
{
  std::vector<const char*> out;
  std::istringstream stream(string);
  std::string extension;
  while (getline(stream, extension, ' '))
  {
    const size_t len = extension.size() + 1u;
    char* str = new char[len];
    memcpy(str, extension.c_str(), len);
    out.push_back(str);
  }

  return out;
}

XrPosef makeIdentity()
{
  XrPosef identity;
  identity.position = { 0.0f, 0.0f, 0.0f };
  identity.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
  return identity;
}

glm::mat4 poseToMatrix(const XrPosef& pose)
{
  const glm::mat4 translation =
    glm::translate(glm::mat4(1.0f), glm::vec3(pose.position.x, pose.position.y, pose.position.z));

  const glm::mat4 rotation =
    glm::toMat4(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z));

  return glm::inverse(translation * rotation);
}

glm::mat4 createProjectionMatrix(XrFovf fov, float nearClip, float farClip)
{
  const float l = glm::tan(fov.angleLeft);
  const float r = glm::tan(fov.angleRight);
  const float d = glm::tan(fov.angleDown);
  const float u = glm::tan(fov.angleUp);

  const float w = r - l;
  const float h = d - u;

  glm::mat4 projectionMatrix;
  projectionMatrix[0] = { 2.0f / w, 0.0f, 0.0f, 0.0f };
  projectionMatrix[1] = { 0.0f, 2.0f / h, 0.0f, 0.0f };
  projectionMatrix[2] = { (r + l) / w, (u + d) / h, -(farClip + nearClip) / (farClip - nearClip), -1.0f };
  projectionMatrix[3] = { 0.0f, 0.0f, -(farClip * (nearClip + nearClip)) / (farClip - nearClip), 0.0f };
  return projectionMatrix;
}

bool onSessionStateReady(const XrSession& session)
{
  // Start session
  XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
  sessionBeginInfo.primaryViewConfigurationType = viewType;
  const XrResult result = xrBeginSession(session, &sessionBeginInfo);
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}

bool onSessionStateStopping(const XrSession& session)
{
  // End session
  const XrResult result = xrEndSession(session);
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}

bool onSessionStateExiting(const XrSession& session, const XrInstance& instance)
{
  // Destroy session
  XrResult result = xrDestroySession(session);
  if (XR_FAILED(result))
  {
    return false;
  }

  // Destroy instance
  result = xrDestroyInstance(instance);
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}
} // namespace

Headset::Headset()
{
  // Initialize GLFW
  if (!glfwInit())
  {
    error = Error::GLFW;
    return;
  }

  if (!glfwVulkanSupported())
  {
    error = Error::GLFW;
    return;
  }

  // Get all supported OpenXR instance extensions
  std::vector<XrExtensionProperties> supportedOpenXRInstanceExtensions;
  {
    uint32_t instanceExtensionCount;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0u, &instanceExtensionCount, nullptr);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    supportedOpenXRInstanceExtensions.resize(instanceExtensionCount);
    for (XrExtensionProperties& extensionProperty : supportedOpenXRInstanceExtensions)
    {
      extensionProperty.type = XR_TYPE_EXTENSION_PROPERTIES;
      extensionProperty.next = nullptr;
    }

    result = xrEnumerateInstanceExtensionProperties(nullptr, instanceExtensionCount, &instanceExtensionCount,
                                                    supportedOpenXRInstanceExtensions.data());
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }
  }

  // Create OpenXR instance
  {
    XrApplicationInfo applicationInfo;
    applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    applicationInfo.applicationVersion = static_cast<uint32_t>(XR_MAKE_VERSION(0, 1, 0));
    applicationInfo.engineVersion = static_cast<uint32_t>(XR_MAKE_VERSION(0, 1, 0));
    strncpy_s(applicationInfo.applicationName, "xrvk", XR_MAX_APPLICATION_NAME_SIZE);
    strncpy_s(applicationInfo.engineName, "xrvk", XR_MAX_ENGINE_NAME_SIZE);

    std::vector<const char*> extensions = { XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };

#ifdef DEBUG
    // Add OpenXR debug instance extension
    extensions.emplace_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // Check that all OpenXR instance extensions are supported
    for (const char* extension : extensions)
    {
      bool extensionSupported = false;
      for (const XrExtensionProperties& supportedExtension : supportedOpenXRInstanceExtensions)
      {
        if (strcmp(extension, supportedExtension.extensionName) == 0)
        {
          extensionSupported = true;
          break;
        }
      }

      if (!extensionSupported)
      {
        error = Error::OpenXR;
        return;
      }
    }

    XrInstanceCreateInfo instanceCreateInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceCreateInfo.enabledExtensionNames = extensions.data();
    instanceCreateInfo.applicationInfo = applicationInfo;
    const XrResult result = xrCreateInstance(&instanceCreateInfo, &xr.instance);
    if (XR_FAILED(result))
    {
      error = Error::NoHeadsetDetected;
      return;
    }
  }

  // Load required OpenXR extension functions
  if (!loadXrExtensionFunction(xr.instance, "xrGetVulkanInstanceExtensionsKHR",
                               reinterpret_cast<PFN_xrVoidFunction*>(&xr.getVulkanInstanceExtensionsKHR)))
  {
    error = Error::OpenXR;
    return;
  }

  if (!loadXrExtensionFunction(xr.instance, "xrGetVulkanDeviceExtensionsKHR",
                               reinterpret_cast<PFN_xrVoidFunction*>(&xr.getVulkanDeviceExtensionsKHR)))
  {
    error = Error::OpenXR;
    return;
  }

  if (!loadXrExtensionFunction(xr.instance, "xrGetVulkanGraphicsDeviceKHR",
                               reinterpret_cast<PFN_xrVoidFunction*>(&xr.getVulkanGraphicsDeviceKHR)))
  {
    error = Error::OpenXR;
    return;
  }

  if (!loadXrExtensionFunction(xr.instance, "xrGetVulkanGraphicsRequirementsKHR",
                               reinterpret_cast<PFN_xrVoidFunction*>(&xr.getVulkanGraphicsRequirementsKHR)))
  {
    error = Error::OpenXR;
    return;
  }

#ifdef DEBUG
  // Create OpenXR debug utils messenger for validation
  {
    if (!loadXrExtensionFunction(xr.instance, "xrCreateDebugUtilsMessengerEXT",
                                 reinterpret_cast<PFN_xrVoidFunction*>(&xr.createDebugUtilsMessengerEXT)))
    {
      error = Error::OpenXR;
      return;
    }

    if (!loadXrExtensionFunction(xr.instance, "xrDestroyDebugUtilsMessengerEXT",
                                 reinterpret_cast<PFN_xrVoidFunction*>(&xr.destroyDebugUtilsMessengerEXT)))
    {
      error = Error::OpenXR;
      return;
    }

    constexpr XrDebugUtilsMessageTypeFlagsEXT typeFlags =
      XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;

    constexpr XrDebugUtilsMessageSeverityFlagsEXT severityFlags =
      XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    const auto callback = [](XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                             XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                             const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) -> XrBool32
    {
      if (messageSeverity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
      {
        std::cerr << "[OpenXR] " << callbackData->message << "\n";
      }

      return XR_FALSE; // Returning XR_TRUE will force the calling function to fail
    };

    XrDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{ XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    debugUtilsMessengerCreateInfo.messageTypes = typeFlags;
    debugUtilsMessengerCreateInfo.messageSeverities = severityFlags;
    debugUtilsMessengerCreateInfo.userCallback = callback;
    const XrResult result =
      xr.createDebugUtilsMessengerEXT(xr.instance, &debugUtilsMessengerCreateInfo, &xr.debugUtilsMessenger);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }
  }
#endif

  // Get system ID
  XrSystemGetInfo systemGetInfo{ XR_TYPE_SYSTEM_GET_INFO };
  systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XrResult result = xrGetSystem(xr.instance, &systemGetInfo, &xr.systemId);
  if (XR_FAILED(result))
  {
    error = Error::NoHeadsetDetected;
    return;
  }

  // Check supported environment blend modes
  {
    uint32_t environmentBlendModeCount;
    result =
      xrEnumerateEnvironmentBlendModes(xr.instance, xr.systemId, viewType, 0u, &environmentBlendModeCount, nullptr);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    std::vector<XrEnvironmentBlendMode> supportedEnvironmentBlendModes(environmentBlendModeCount);
    result = xrEnumerateEnvironmentBlendModes(xr.instance, xr.systemId, viewType, environmentBlendModeCount,
                                              &environmentBlendModeCount, supportedEnvironmentBlendModes.data());
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    bool modeFound = false;
    for (const XrEnvironmentBlendMode& mode : supportedEnvironmentBlendModes)
    {
      if (mode == environmentBlendMode)
      {
        modeFound = true;
        break;
      }
    }

    if (!modeFound)
    {
      error = Error::OpenXR;
      return;
    }
  }

  // Get all supported Vulkan instance extensions
  std::vector<VkExtensionProperties> supportedVulkanInstanceExtensions;
  {
    uint32_t instanceExtensionCount;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }

    supportedVulkanInstanceExtensions.resize(instanceExtensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount,
                                               supportedVulkanInstanceExtensions.data()) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }

  // Get required Vulkan instance extensions from GLFW
  std::vector<const char*> vulkanInstanceExtensions;
  {
    uint32_t requiredExtensionCount;
    const char** buffer = glfwGetRequiredInstanceExtensions(&requiredExtensionCount);
    if (!buffer)
    {
      error = Error::GLFW;
      return;
    }

    for (uint32_t i = 0u; i < requiredExtensionCount; ++i)
    {
      vulkanInstanceExtensions.emplace_back(buffer[i]);
    }
  }

  // Get required Vulkan instance extensions from OpenXR and add them
  {
    uint32_t count;
    result = xr.getVulkanInstanceExtensionsKHR(xr.instance, xr.systemId, 0u, &count, nullptr);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    std::string buffer;
    buffer.resize(count);
    result = xr.getVulkanInstanceExtensionsKHR(xr.instance, xr.systemId, count, &count, buffer.data());
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    const std::vector<const char*> instanceExtensions = extensionStringToVector(buffer);
    for (const char* extension : instanceExtensions)
    {
      vulkanInstanceExtensions.emplace_back(extension);
    }
  }

#ifdef DEBUG
  // Add Vulkan debug instance extension
  vulkanInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  // Check that all required Vulkan instance extensions are supported
  {
    for (const char* extension : vulkanInstanceExtensions)
    {
      bool extensionSupported = false;

      for (const VkExtensionProperties& supportedExtension : supportedVulkanInstanceExtensions)
      {
        if (strcmp(extension, supportedExtension.extensionName) == 0)
        {
          extensionSupported = true;
          break;
        }
      }

      if (!extensionSupported)
      {
        error = Error::Vulkan;
        return;
      }
    }
  }

  // Create Vulkan instance with all required extensions
  {
    VkApplicationInfo applicationInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    applicationInfo.apiVersion = VK_API_VERSION_1_3;
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.pApplicationName = "xrvk";
    applicationInfo.pEngineName = "xrvk";

    VkInstanceCreateInfo instanceCreateInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(vulkanInstanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = vulkanInstanceExtensions.data();

#ifdef DEBUG
    constexpr std::array layers = { "VK_LAYER_KHRONOS_validation" };

    // Get all supported Vulkan instance layers
    std::vector<VkLayerProperties> supportedInstanceLayers;
    uint32_t instanceLayerCount;
    if (vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }

    supportedInstanceLayers.resize(instanceLayerCount);
    if (vkEnumerateInstanceLayerProperties(&instanceLayerCount, supportedInstanceLayers.data()) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }

    // Check that all Vulkan instance layers are supported
    for (const char* layer : layers)
    {
      bool layerSupported = false;
      for (const VkLayerProperties& supportedLayer : supportedInstanceLayers)
      {
        if (strcmp(layer, supportedLayer.layerName) == 0)
        {
          layerSupported = true;
          break;
        }
      }

      if (!layerSupported)
      {
        error = Error::Vulkan;
        return;
      }
    }

    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceCreateInfo.ppEnabledLayerNames = layers.data();
#endif

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &vk.instance) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }

#ifdef DEBUG
  // Create Vulkan debug utils messenger for validation
  {
    vk.createDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      loadVkExtensionFunction(vk.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!vk.createDebugUtilsMessengerEXT)
    {
      error = Error::Vulkan;
      return;
    }

    vk.destroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      loadVkExtensionFunction(vk.instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (!vk.destroyDebugUtilsMessengerEXT)
    {
      error = Error::Vulkan;
      return;
    }

    constexpr XrDebugUtilsMessageTypeFlagsEXT typeFlags = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    constexpr XrDebugUtilsMessageSeverityFlagsEXT severityFlags =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    const auto callback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                             VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                             const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) -> VkBool32
    {
      if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
      {
        std::cerr << "[Vulkan] " << callbackData->pMessage << "\n";
      }

      return VK_FALSE; // Returning VK_TRUE will force the calling function to fail
    };

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
    };
    debugUtilsMessengerCreateInfo.messageType = typeFlags;
    debugUtilsMessengerCreateInfo.messageSeverity = severityFlags;
    debugUtilsMessengerCreateInfo.pfnUserCallback = callback;
    if (vk.createDebugUtilsMessengerEXT(vk.instance, &debugUtilsMessengerCreateInfo, nullptr,
                                        &vk.debugUtilsMessenger) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }
#endif

  // Retrieve physical device from OpenXR
  result = xr.getVulkanGraphicsDeviceKHR(xr.instance, xr.systemId, vk.instance, &vk.physicalDevice);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Pick draw queue family index
  {
    // Retrieve queue families
    std::vector<VkQueueFamilyProperties> queueFamilies;
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(vk.physicalDevice, &queueFamilyCount, nullptr);

    queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vk.physicalDevice, &queueFamilyCount, queueFamilies.data());

    bool queueFamilyIndexFound = false;
    for (size_t queueFamilyIndexCandidate = 0u; queueFamilyIndexCandidate < queueFamilies.size();
         ++queueFamilyIndexCandidate)
    {
      const VkQueueFamilyProperties& queueFamilyCandidate = queueFamilies.at(queueFamilyIndexCandidate);

      // Check that queue family includes actual queues
      if (queueFamilyCandidate.queueCount == 0u)
      {
        continue;
      }

      // Check queue family for drawing support
      if (queueFamilyCandidate.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        vk.queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndexCandidate);
        queueFamilyIndexFound = true;
        break;
      }
    }

    if (!queueFamilyIndexFound)
    {
      error = Error::Vulkan;
      return;
    }
  }

  // Get all supported Vulkan device extensions
  std::vector<VkExtensionProperties> supportedVulkanDeviceExtensions;
  {
    uint32_t deviceExtensionCount;
    if (vkEnumerateDeviceExtensionProperties(vk.physicalDevice, nullptr, &deviceExtensionCount, nullptr) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }

    supportedVulkanDeviceExtensions.resize(deviceExtensionCount);
    if (vkEnumerateDeviceExtensionProperties(vk.physicalDevice, nullptr, &deviceExtensionCount,
                                             supportedVulkanDeviceExtensions.data()) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }

  // Get required Vulkan device extensions from OpenXR
  std::vector<const char*> vulkanDeviceExtensions;
  {
    uint32_t count;
    XrResult result = xr.getVulkanDeviceExtensionsKHR(xr.instance, xr.systemId, 0u, &count, nullptr);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    std::string buffer;
    buffer.resize(count);
    result = xr.getVulkanDeviceExtensionsKHR(xr.instance, xr.systemId, count, &count, buffer.data());
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    vulkanDeviceExtensions = extensionStringToVector(buffer);
  }

  // Add required swapchain extension for mirror view
  vulkanDeviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  // Check that all Vulkan device extensions are supported
  {
    for (const char* extension : vulkanDeviceExtensions)
    {
      bool extensionSupported = false;
      for (const VkExtensionProperties& supportedExtension : supportedVulkanDeviceExtensions)
      {
        if (strcmp(extension, supportedExtension.extensionName) == 0)
        {
          extensionSupported = true;
          break;
        }
      }

      if (!extensionSupported)
      {
        error = Error::Vulkan;
        return;
      }
    }
  }

  // Create device
  {
    constexpr float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo deviceQueueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    deviceQueueCreateInfo.queueFamilyIndex = vk.queueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1u;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    // Verify that required physical device features are supported
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(vk.physicalDevice, &physicalDeviceFeatures);
    if (!physicalDeviceFeatures.shaderStorageImageMultisample)
    {
      error = Error::Vulkan;
      return;
    }

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceMultiviewFeatures physicalDeviceMultiviewFeatures{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES
    };
    physicalDeviceFeatures2.pNext = &physicalDeviceMultiviewFeatures;
    vkGetPhysicalDeviceFeatures2(vk.physicalDevice, &physicalDeviceFeatures2);
    if (!physicalDeviceMultiviewFeatures.multiview)
    {
      error = Error::Vulkan;
      return;
    }

    physicalDeviceFeatures.shaderStorageImageMultisample = VK_TRUE; // For OpenXR to avoid a validation error
    physicalDeviceMultiviewFeatures.multiview = VK_TRUE;            // For stereo rendering

    VkDeviceCreateInfo deviceCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(vulkanDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = vulkanDeviceExtensions.data();
    deviceCreateInfo.queueCreateInfoCount = 1u;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;
    deviceCreateInfo.pNext = &physicalDeviceMultiviewFeatures;
    if (vkCreateDevice(vk.physicalDevice, &deviceCreateInfo, nullptr, &vk.device) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }

  // Check graphics requirements for Vulkan
  XrGraphicsRequirementsVulkanKHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };
  result = xr.getVulkanGraphicsRequirementsKHR(xr.instance, xr.systemId, &graphicsRequirements);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  vkGetDeviceQueue(vk.device, vk.queueFamilyIndex, 0u, &vk.queue);

  // Create render pass
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

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1u;
    subpassDescription.pColorAttachments = &colorAttachmentReference;

    VkRenderPassCreateInfo renderPassCreateInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassCreateInfo.pNext = &renderPassMultiviewCreateInfo;
    renderPassCreateInfo.attachmentCount = 1u;
    renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
    renderPassCreateInfo.subpassCount = 1u;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    if (vkCreateRenderPass(vk.device, &renderPassCreateInfo, nullptr, &vk.renderPass) != VK_SUCCESS)
    {
      error = Error::Vulkan;
      return;
    }
  }

  // Create session with Vulkan graphics binding
  XrGraphicsBindingVulkanKHR graphicsBinding{ XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR };
  graphicsBinding.device = vk.device;
  graphicsBinding.instance = vk.instance;
  graphicsBinding.physicalDevice = vk.physicalDevice;
  graphicsBinding.queueFamilyIndex = vk.queueFamilyIndex;
  graphicsBinding.queueIndex = 0u;

  XrSessionCreateInfo sessionCreateInfo{ XR_TYPE_SESSION_CREATE_INFO };
  sessionCreateInfo.next = &graphicsBinding;
  sessionCreateInfo.systemId = xr.systemId;
  result = xrCreateSession(xr.instance, &sessionCreateInfo, &xr.session);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Create play space
  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
  referenceSpaceCreateInfo.referenceSpaceType = spaceType;
  referenceSpaceCreateInfo.poseInReferenceSpace = makeIdentity();
  result = xrCreateReferenceSpace(xr.session, &referenceSpaceCreateInfo, &xr.space);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Get number of eyes
  result = xrEnumerateViewConfigurationViews(xr.instance, xr.systemId, viewType, 0u,
                                             reinterpret_cast<uint32_t*>(&eyeCount), nullptr);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Get eye image info per eye
  xr.eyeImageInfos.resize(eyeCount);
  for (XrViewConfigurationView& eyeInfo : xr.eyeImageInfos)
  {
    eyeInfo.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    eyeInfo.next = nullptr;
  }

  result = xrEnumerateViewConfigurationViews(xr.instance, xr.systemId, viewType,
                                             static_cast<uint32_t>(xr.eyeImageInfos.size()),
                                             reinterpret_cast<uint32_t*>(&eyeCount), xr.eyeImageInfos.data());
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }

  // Allocate eye poses
  xr.eyePoses.resize(eyeCount);
  for (XrView& eyePose : xr.eyePoses)
  {
    eyePose.type = XR_TYPE_VIEW;
    eyePose.next = nullptr;
  }

  // Verify that desired color format is supported
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

  // Create swapchain and render targets
  {
    const XrViewConfigurationView& eyeImageInfo = xr.eyeImageInfos.at(0u);

    // Create swapchain
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

    // Get number of swapchain images
    uint32_t swapchainImageCount;
    result = xrEnumerateSwapchainImages(xr.swapchain, 0u, &swapchainImageCount, nullptr);
    if (XR_FAILED(result))
    {
      error = Error::OpenXR;
      return;
    }

    // Retrieve swapchain images
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

    // Create render target
    xr.swapchainRenderTargets.resize(swapchainImages.size());
    for (size_t renderTargetIndex = 0u; renderTargetIndex < xr.swapchainRenderTargets.size(); ++renderTargetIndex)
    {
      RenderTarget*& renderTarget = xr.swapchainRenderTargets.at(renderTargetIndex);

      const VkImage image = swapchainImages.at(renderTargetIndex).image;
      renderTarget = new RenderTarget(vk.device, image, getEyeResolution(0u), colorFormat, vk.renderPass, 2u);
      if (!renderTarget->isValid())
      {
        error = Error::Vulkan;
        return;
      }
    }
  }

  // Create eye render info
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

void Headset::sync() const
{
  vkDeviceWaitIdle(vk.device);
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

#ifdef DEBUG
  xr.destroyDebugUtilsMessengerEXT(xr.debugUtilsMessenger);
#endif

  xrDestroyInstance(xr.instance);

  // Clean up Vulkan
  vkDestroyRenderPass(vk.device, vk.renderPass, nullptr);
  vkDestroyDevice(vk.device, nullptr);

#ifdef DEBUG
  vk.destroyDebugUtilsMessengerEXT(vk.instance, vk.debugUtilsMessenger, nullptr);
#endif

  vkDestroyInstance(vk.instance, nullptr);
}

Headset::BeginFrameResult Headset::beginFrame(uint32_t& swapchainImageIndex)
{
  // Poll OpenXR events
  XrEventDataBuffer buffer;
  buffer.type = XR_TYPE_EVENT_DATA_BUFFER;
  while (xrPollEvent(xr.instance, &buffer) == XR_SUCCESS)
  {
    switch (buffer.type)
    {
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
      onSessionStateExiting(xr.session, xr.instance);
      break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
    {
      XrEventDataSessionStateChanged* event = reinterpret_cast<XrEventDataSessionStateChanged*>(&buffer);
      xr.sessionState = event->state;

      if (event->state == XR_SESSION_STATE_READY)
      {
        if (!onSessionStateReady(xr.session))
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_STOPPING)
      {
        if (!onSessionStateStopping(xr.session))
        {
          return BeginFrameResult::Error;
        }
      }
      else if (event->state == XR_SESSION_STATE_LOSS_PENDING || event->state == XR_SESSION_STATE_EXITING)
      {
        if (!onSessionStateExiting(xr.session, xr.instance))
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

  // Wait for new frame
  xr.frameState.type = XR_TYPE_FRAME_STATE;
  XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
  XrResult result = xrWaitFrame(xr.session, &frameWaitInfo, &xr.frameState);
  if (XR_FAILED(result))
  {
    return BeginFrameResult::Error;
  }

  // Begin new frame
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
    return BeginFrameResult::SkipButEnd;
  }

  // Update eye poses
  xr.viewState.type = XR_TYPE_VIEW_STATE;
  uint32_t viewCount;
  XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
  viewLocateInfo.viewConfigurationType = viewType;
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

  // Update eye render info, view and projection matrices
  for (size_t eyeIndex = 0u; eyeIndex < eyeCount; ++eyeIndex)
  {
    // Copy eye pose into eye render info
    XrCompositionLayerProjectionView& eyeRenderInfo = xr.eyeRenderInfos.at(eyeIndex);
    const XrView& eyePose = xr.eyePoses.at(eyeIndex);
    eyeRenderInfo.pose = eyePose.pose;
    eyeRenderInfo.fov = eyePose.fov;

    // Update view matrix
    const XrPosef& pose = eyeRenderInfo.pose;
    eyeViewMatrices.at(eyeIndex) = poseToMatrix(pose);

    // Update projection matrix
    eyeProjectionMatrices.at(eyeIndex) = createProjectionMatrix(eyeRenderInfo.fov, 0.1f, 250.0f);
  }

  // Acquire swapchain image
  XrSwapchainImageAcquireInfo swapchainImageAcquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
  result = xrAcquireSwapchainImage(xr.swapchain, &swapchainImageAcquireInfo, &swapchainImageIndex);
  if (XR_FAILED(result))
  {
    return BeginFrameResult::Error;
  }

  // Wait for swapchain image
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
  // Release swapchain image
  XrSwapchainImageReleaseInfo swapchainImageReleaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
  XrResult result = xrReleaseSwapchainImage(xr.swapchain, &swapchainImageReleaseInfo);
  if (XR_FAILED(result))
  {
    return;
  }

  // End frame
  XrCompositionLayerProjection compositionLayerProjection{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
  compositionLayerProjection.space = xr.space;
  compositionLayerProjection.viewCount = static_cast<uint32_t>(xr.eyeRenderInfos.size());
  compositionLayerProjection.views = xr.eyeRenderInfos.data();

  uint32_t submittedLayerCount = 1u;
  const XrCompositionLayerBaseHeader* submittedLayers[1] = {
    reinterpret_cast<const XrCompositionLayerBaseHeader* const>(&compositionLayerProjection)
  };

  if (!xr.frameState.shouldRender || (xr.viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0u ||
      (xr.viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0u)
  {
    submittedLayerCount = 0u;
  }

  XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
  frameEndInfo.displayTime = xr.frameState.predictedDisplayTime;
  frameEndInfo.layerCount = submittedLayerCount;
  frameEndInfo.layers = (submittedLayerCount == 0u ? nullptr : submittedLayers);
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

VkInstance Headset::getInstance() const
{
  return vk.instance;
}

VkPhysicalDevice Headset::getPhysicalDevice() const
{
  return vk.physicalDevice;
}

VkDevice Headset::getDevice() const
{
  return vk.device;
}

VkRenderPass Headset::getRenderPass() const
{
  return vk.renderPass;
}

uint32_t Headset::getQueueFamilyIndex() const
{
  return vk.queueFamilyIndex;
}

VkQueue Headset::getQueue() const
{
  return vk.queue;
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