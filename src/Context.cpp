#include "Context.h"

#include "Util.h"

#include <glfw/glfw3.h>

#ifdef DEBUG
  #include <array>
  #include <iostream>
#endif

namespace
{
inline constexpr XrViewConfigurationType viewType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
inline constexpr XrEnvironmentBlendMode environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
} // namespace

Context::Context()
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

  // Create an OpenXR instance
  {
    XrApplicationInfo applicationInfo;
    applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    applicationInfo.applicationVersion = static_cast<uint32_t>(XR_MAKE_VERSION(0, 1, 0));
    applicationInfo.engineVersion = static_cast<uint32_t>(XR_MAKE_VERSION(0, 1, 0));
    strncpy_s(applicationInfo.applicationName, "xrvk", XR_MAX_APPLICATION_NAME_SIZE);
    strncpy_s(applicationInfo.engineName, "xrvk", XR_MAX_ENGINE_NAME_SIZE);

    std::vector<const char*> extensions = { XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };

#ifdef DEBUG
    // Add the OpenXR debug instance extension
    extensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
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

  // Load the required OpenXR extension functions
  if (!util::loadXrExtensionFunction(xr.instance, "xrGetVulkanInstanceExtensionsKHR",
                                     reinterpret_cast<PFN_xrVoidFunction*>(&xr.getVulkanInstanceExtensionsKHR)))
  {
    error = Error::OpenXR;
    return;
  }

  if (!util::loadXrExtensionFunction(xr.instance, "xrGetVulkanGraphicsDeviceKHR",
                                     reinterpret_cast<PFN_xrVoidFunction*>(&xr.getVulkanGraphicsDeviceKHR)))
  {
    error = Error::OpenXR;
    return;
  }

#ifdef DEBUG
  // Create an OpenXR debug utils messenger for validation
  {
    if (!util::loadXrExtensionFunction(xr.instance, "xrCreateDebugUtilsMessengerEXT",
                                       reinterpret_cast<PFN_xrVoidFunction*>(&xr.createDebugUtilsMessengerEXT)))
    {
      error = Error::OpenXR;
      return;
    }

    if (!util::loadXrExtensionFunction(xr.instance, "xrDestroyDebugUtilsMessengerEXT",
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

  // Get the system ID
  XrSystemGetInfo systemGetInfo{ XR_TYPE_SYSTEM_GET_INFO };
  systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XrResult result = xrGetSystem(xr.instance, &systemGetInfo, &xr.systemId);
  if (XR_FAILED(result))
  {
    error = Error::NoHeadsetDetected;
    return;
  }

  // Check the supported environment blend modes
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

  // Get the required Vulkan instance extensions from GLFW
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
      vulkanInstanceExtensions.push_back(buffer[i]);
    }
  }

  // Get the required Vulkan instance extensions from OpenXR and add them
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

    const std::vector<const char*> instanceExtensions = util::unpackExtensionString(buffer);
    for (const char* extension : instanceExtensions)
    {
      vulkanInstanceExtensions.push_back(extension);
    }
  }

#ifdef DEBUG
  // Add the Vulkan debug instance extension
  vulkanInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  // Check that all required Vulkan instance extensions are supported
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

  // Create a Vulkan instance with all required extensions
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
  // Create a Vulkan debug utils messenger for validation
  {
    vk.createDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      util::loadVkExtensionFunction(vk.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!vk.createDebugUtilsMessengerEXT)
    {
      error = Error::Vulkan;
      return;
    }

    vk.destroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      util::loadVkExtensionFunction(vk.instance, "vkDestroyDebugUtilsMessengerEXT"));
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

  // Retrieve the physical device from OpenXR
  result = xr.getVulkanGraphicsDeviceKHR(xr.instance, xr.systemId, vk.instance, &vk.physicalDevice);
  if (XR_FAILED(result))
  {
    error = Error::OpenXR;
    return;
  }
}

void Context::destroy() const
{
  // Clean up OpenXR
#ifdef DEBUG
  xr.destroyDebugUtilsMessengerEXT(xr.debugUtilsMessenger);
#endif

  xrDestroyInstance(xr.instance);

  // Clean up Vulkan
#ifdef DEBUG
  vk.destroyDebugUtilsMessengerEXT(vk.instance, vk.debugUtilsMessenger, nullptr);
#endif

  vkDestroyInstance(vk.instance, nullptr);
}

Context::Error Context::getError() const
{
  return error;
}

XrViewConfigurationType Context::getXrViewType() const
{
  return viewType;
}

XrInstance Context::getXrInstance() const
{
  return xr.instance;
}

XrSystemId Context::getXrSystemId() const
{
  return xr.systemId;
}

VkInstance Context::getVkInstance() const
{
  return vk.instance;
}

VkPhysicalDevice Context::getVkPhysicalDevice() const
{
  return vk.physicalDevice;
}
