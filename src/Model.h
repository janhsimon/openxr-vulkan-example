#pragma once

#include <glm/mat4x4.hpp>

struct Model final
{
  size_t firstIndex = 0u;
  size_t numIndices = 0u;
  glm::mat4 worldMatrix;
};