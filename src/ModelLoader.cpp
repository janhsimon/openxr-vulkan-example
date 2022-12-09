#include "ModelLoader.h"

#include "Util.h"

#include <tinyobjloader/tiny_obj_loader.h>

bool ModelLoader::loadModel(const std::string& filename, Color color)
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, nullptr, nullptr, filename.c_str()))
  {
    util::error(Error::ModelLoadingFailure, filename);
    return false;
  }

  const size_t firstVertex = vertices.size();

  for (size_t i = 0u; i < attrib.vertices.size(); i += 3u)
  {
    Vertex vertex;
    vertex.position = { attrib.vertices.at(i + 0u), attrib.vertices.at(i + 1u), attrib.vertices.at(i + 2u) };

    switch (color)
    {
    case Color::White:
      vertex.color = { 1.0f, 1.0f, 1.0f };
      break;

    case Color::Generate:
    {
      const size_t index = i / 3u;
      const float r = index % 3u == 0u;
      const float g = index % 3u == 1u;
      const float b = index % 4u >= 2u;
      vertex.color = { r, g, b };
      break;
    }
    }

    vertices.push_back(vertex);
  }

  const size_t oldIndexCount = indices.size();
  for (const tinyobj::shape_t& shape : shapes)
  {
    for (const tinyobj::index_t& index : shape.mesh.indices)
    {
      indices.push_back(static_cast<uint16_t>(static_cast<size_t>(index.vertex_index) + firstVertex));
    }
  }

  numIndicesPerModel.push_back(indices.size() - oldIndexCount);

  return true;
}

size_t ModelLoader::getVerticesSize() const
{
  return sizeof(vertices.at(0u)) * vertices.size();
}

const Vertex* ModelLoader::getVerticesData() const
{
  return vertices.data();
}

size_t ModelLoader::getNumIndices() const
{
  return indices.size();
}

size_t ModelLoader::getNumIndicesPerModel(size_t model) const
{
  return numIndicesPerModel.at(model);
}

size_t ModelLoader::getIndicesSize() const
{
  return sizeof(indices.at(0u)) * indices.size();
}

const uint16_t* ModelLoader::getIndicesData() const
{
  return indices.data();
}