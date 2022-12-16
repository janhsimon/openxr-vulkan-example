#include "MeshData.h"

#include "Model.h"
#include "Util.h"

#include <tinyobjloader/tiny_obj_loader.h>

bool MeshData::loadModel(const std::string& filename,
                         Color color,
                         std::vector<Model*>& models,
                         size_t offset,
                         size_t count)
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  if (!tinyobj::LoadObj(&attrib, &shapes, nullptr, nullptr, nullptr, filename.c_str()))
  {
    util::error(Error::ModelLoadingFailure, filename);
    return false;
  }

  const size_t oldIndexCount = indices.size();

  for (const tinyobj::shape_t& shape : shapes)
  {
    for (size_t i = 0u; i < shape.mesh.indices.size(); ++i)
    {
      const tinyobj::index_t index = shape.mesh.indices.at(i);

      Vertex vertex;

      vertex.position = { attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                          attrib.vertices[3 * index.vertex_index + 2] };

      if (index.normal_index >= 0)
      {
        vertex.normal = { attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1],
                          attrib.normals[3 * index.normal_index + 2] };
      }
      else
      {
        vertex.normal = { 0.0f, 0.0f, 0.0f };
      }

      switch (color)
      {
      case Color::White:
        vertex.color = { 1.0f, 1.0f, 1.0f };
        break;
      case Color::FromNormals:
        vertex.color = vertex.normal;
        break;
      }

      vertices.push_back(vertex);
      indices.push_back(static_cast<uint32_t>(indices.size()));
    }
  }

  for (size_t modelIndex = offset; modelIndex < offset + count; ++modelIndex)
  {
    Model* model = models.at(modelIndex);
    model->firstIndex = oldIndexCount;
    model->numIndices = indices.size() - oldIndexCount;
  }

  return true;
}

size_t MeshData::getSize() const
{
  return sizeof(vertices.at(0u)) * vertices.size() + sizeof(indices.at(0u)) * indices.size();
}

size_t MeshData::getIndexOffset() const
{
  return sizeof(vertices.at(0u)) * vertices.size();
}

void MeshData::writeTo(char* destination) const
{
  const size_t verticesSize = sizeof(vertices.at(0u)) * vertices.size();
  const size_t indicesSize = sizeof(indices.at(0u)) * indices.size();
  memcpy(destination, vertices.data(), verticesSize);              // Vertex section first
  memcpy(destination + verticesSize, indices.data(), indicesSize); // Index section next
}