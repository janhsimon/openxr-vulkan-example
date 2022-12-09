#pragma once

#include "Vertex.h"

#include <string>
#include <vector>

class ModelLoader final
{
public:
  enum class Color
  {
    White,
    Generate
  };
  bool loadModel(const std::string& filename, Color color);

  size_t getVerticesSize() const;
  const Vertex* getVerticesData() const;

  size_t getNumIndices() const;
  size_t getNumIndicesPerModel(size_t model) const;
  size_t getIndicesSize() const;
  const uint16_t* getIndicesData() const;

private:
  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
  std::vector<size_t> numIndicesPerModel;
};