#include "Util.h"

#include <glm/gtx/quaternion.hpp>

#include <fstream>
#include <sstream>

bool util::loadXrExtensionFunction(XrInstance instance, const std::string& name, PFN_xrVoidFunction* function)
{
  const XrResult result = xrGetInstanceProcAddr(instance, name.c_str(), function);
  if (XR_FAILED(result))
  {
    return false;
  }

  return true;
}

PFN_vkVoidFunction util::loadVkExtensionFunction(VkInstance instance, const std::string& name)
{
  return vkGetInstanceProcAddr(instance, name.c_str());
}

std::vector<const char*> util::unpackExtensionString(const std::string& string)
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

bool util::loadShaderFromFile(VkDevice device, const std::string& filename, VkShaderModule& shaderModule)
{
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open())
  {
    return false;
  }

  const size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> code(fileSize);
  file.seekg(0);
  file.read(code.data(), fileSize);
  file.close();

  VkShaderModuleCreateInfo shaderModuleCreateInfo;
  shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCreateInfo.pNext = nullptr;
  shaderModuleCreateInfo.flags = 0u;
  shaderModuleCreateInfo.codeSize = code.size();
  shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
  if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
  {
    return false;
  }

  return true;
}

XrPosef util::makeIdentity()
{
  XrPosef identity;
  identity.position = { 0.0f, 0.0f, 0.0f };
  identity.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
  return identity;
}

glm::mat4 util::poseToMatrix(const XrPosef& pose)
{
  const glm::mat4 translation =
    glm::translate(glm::mat4(1.0f), glm::vec3(pose.position.x, pose.position.y, pose.position.z));

  const glm::mat4 rotation =
    glm::toMat4(glm::quat(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z));

  return glm::inverse(translation * rotation);
}

glm::mat4 util::createProjectionMatrix(XrFovf fov, float nearClip, float farClip)
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