#pragma once

#include <glm/mat4x4.hpp>

/*
 * The model struct holds all required information to orientate and render a model. It handles orientation with a world
 * transformation matrix and has its indexing information populated by the mesh data class. This struct represents a
 * single draw call and is used by the renderer class to know how and where to draw a model.
 */
struct Model final
{
  size_t firstIndex = 0u;
  size_t indexCount = 0u;
  glm::mat4 worldMatrix;
};