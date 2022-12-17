#pragma once

#include <glm/vec3.hpp>

#include <string>
#include <vector>

struct Model;

/*
 * The vertex struct provides the vertex definition used for all geometry in the project.
 */
struct Vertex final
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
};

/*
 * The mesh data class consists of a vertex and index collection for geometric data. It is not intended to stay alive in
 * memory after loading is done. It's purpose is rather to serve as a container for geometry data read in from OBJ model
 * files until that gets uploaded to a Vulkan vertex/index buffer on the GPU. Note that the models in the mesh data
 * class should be unique, a model that is rendered several times only needs to be loaded once. As many model structs as
 * required can then be derived from the same data.
 */
class MeshData final
{
public:
  enum class Color
  {
    White,
    FromNormals
  };
  bool loadModel(const std::string& filename, Color color, std::vector<Model*>& models, size_t offset, size_t count);

  size_t getSize() const;
  size_t getIndexOffset() const;

  void writeTo(char* destination) const;

private:
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
};