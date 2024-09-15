#pragma once

#include <glm/glm.hpp>

#include <tiny_gltf.h>

#include <etna/Buffer.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <etna/VertexInput.hpp>
#include <etna/OneShotCmdMgr.hpp>

// A single render element (relem) corresponds to a single draw call
// of a certain pipeline with specific bindings (including material data)
struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;

  std::optional<std::size_t> material;
};

// A mesh is a collection of relems. A scene may have the same mesh
// located in several different places, so a scene consists of **instances**,
// not meshes.
struct Mesh
{
  std::uint32_t firstRelem;
  std::uint32_t relemCount;
};

struct MeshInstance
{
  std::uint32_t node;
  std::uint32_t mesh;
};

class SceneMeshes
{
public:
  void load(
    const tinygltf::Model& model,
    etna::BlockingTransferHelper& transfer_helper,
    etna::OneShotCmdMgr& one_shot_cmd_mgr);

  std::span<const MeshInstance> getInstanceMeshes() const { return meshInstances; }

  // Every mesh is a collection of relems
  std::span<const Mesh> getMeshes() const { return meshes; }

  // Every relem is a single draw call
  std::span<const RenderElement> getRenderElements() const { return renderElements; }

  vk::Buffer getVertexBuffer() const { return vertexBuffer.get(); }
  vk::Buffer getIndexBuffer() const { return indexBuffer.get(); }

  etna::VertexByteStreamFormatDescription getVertexFormatDescription() const;

private:
  struct Vertex
  {
    // First 3 floats are position, 4th float is a packed normal
    glm::vec4 positionAndNormal;
    // First 2 floats are tex coords, 3rd is a packed tangent, 4th is padding
    glm::vec4 texCoordAndTangentAndPadding;
  };

  static_assert(sizeof(Vertex) == sizeof(float) * 8);

  std::vector<RenderElement> renderElements;
  std::vector<Mesh> meshes;
  std::vector<MeshInstance> meshInstances;

  etna::Buffer vertexBuffer;
  etna::Buffer indexBuffer;
};
