#pragma once

#include <glm/vec3.hpp>

#include <string>
#include <vector>

struct Model;

struct Vertex final
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
};

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
  std::vector<uint16_t> indices;
};