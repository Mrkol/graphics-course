#include "SceneManager.hpp"
#include "etna/Etna.hpp"
#include "etna/RenderTargetStates.hpp"

#include <stack>

#include <spdlog/spdlog.h>
#include <fmt/std.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <etna/Image.hpp>

#include "stb_image.h"

SceneManager::SceneManager()
  : oneShotCommands{etna::get_context().createOneShotCmdMgr()}
  , transferHelper{etna::BlockingTransferHelper::CreateInfo{.stagingSize = 4096 * 4096 * 4}}
{
}

std::optional<tinygltf::Model> SceneManager::loadModel(std::filesystem::path path)
{
  tinygltf::Model model;

  std::string error;
  std::string warning;
  bool success = false;

  auto ext = path.extension();
  if (ext == ".gltf")
    success = loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
  else if (ext == ".glb")
    success = loader.LoadBinaryFromFile(&model, &error, &warning, path.string());
  else
  {
    spdlog::error("glTF: Unknown glTF file extension: '{}'. Expected .gltf or .glb.", ext);
    return std::nullopt;
  }

  if (!success)
  {
    spdlog::error("glTF: Failed to load model!");
    if (!error.empty())
      spdlog::error("glTF: {}", error);
    return std::nullopt;
  }

  if (!warning.empty())
    spdlog::warn("glTF: {}", warning);

  if (
    !model.extensions.empty() || !model.extensionsRequired.empty() || !model.extensionsUsed.empty())
    spdlog::warn("glTF: No glTF extensions are currently implemented!");

  return model;
}

SceneManager::ProcessedInstances SceneManager::processInstances(const tinygltf::Model& model) const
{
  std::vector nodeTransforms(model.nodes.size(), glm::identity<glm::mat4x4>());

  for (std::size_t nodeIdx = 0; nodeIdx < model.nodes.size(); ++nodeIdx)
  {
    const auto& node = model.nodes[nodeIdx];
    auto& transform = nodeTransforms[nodeIdx];

    if (!node.matrix.empty())
    {
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
          transform[i][j] = static_cast<float>(node.matrix[4 * i + j]);
    }
    else
    {
      if (!node.scale.empty())
        transform = scale(
          transform,
          glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])));

      if (!node.rotation.empty())
        transform *= mat4_cast(glm::quat(
          static_cast<float>(node.rotation[3]),
          static_cast<float>(node.rotation[0]),
          static_cast<float>(node.rotation[1]),
          static_cast<float>(node.rotation[2])));

      if (!node.translation.empty())
        transform = translate(
          transform,
          glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])));
    }
  }

  std::stack<std::size_t> vertices;
  for (auto vert : model.scenes[model.defaultScene].nodes)
    vertices.push(vert);

  while (!vertices.empty())
  {
    auto vert = vertices.top();
    vertices.pop();

    for (auto child : model.nodes[vert].children)
    {
      nodeTransforms[child] = nodeTransforms[vert] * nodeTransforms[child];
      vertices.push(child);
    }
  }

  ProcessedInstances result;

  // Don't overallocate matrices, they are pretty chonky.
  {
    std::size_t totalNodesWithMeshes = 0;
    for (std::size_t i = 0; i < model.nodes.size(); ++i)
      if (model.nodes[i].mesh >= 0)
        ++totalNodesWithMeshes;
    result.matrices.reserve(totalNodesWithMeshes);
    result.meshes.reserve(totalNodesWithMeshes);
  }

  for (std::size_t i = 0; i < model.nodes.size(); ++i)
    if (model.nodes[i].mesh >= 0)
    {
      result.matrices.push_back(nodeTransforms[i]);
      result.meshes.push_back(model.nodes[i].mesh);
    }

  return result;
}

static std::uint32_t encode_normal(glm::vec3 normal)
{
  const std::int32_t x = static_cast<std::int32_t>(normal.x * 32767.0f);
  const std::int32_t y = static_cast<std::int32_t>(normal.y * 32767.0f);

  const std::uint32_t sign = normal.z >= 0 ? 0 : 1;
  const std::uint32_t sx = static_cast<std::uint32_t>(x & 0xfffe) | sign;
  const std::uint32_t sy = static_cast<std::uint32_t>(y & 0xffff) << 16;

  return sx | sy;
}


glm::mat2x3 SceneManager::getBounds(std::span<const SceneManager::Vertex> vtx) {
  glm::vec3 min = vtx[0].positionAndNormal;
  glm::vec3 max = vtx[0].positionAndNormal;
  for (const auto& v : vtx) {
    min.x = std::min(min.x, v.positionAndNormal.x);
    min.y = std::min(min.y, v.positionAndNormal.y);
    min.z = std::min(min.z, v.positionAndNormal.z);

    max.x = std::max(max.x, v.positionAndNormal.x);
    max.y = std::max(max.y, v.positionAndNormal.y);
    max.z = std::max(max.z, v.positionAndNormal.z);
  }
  glm::vec3 center = (max + min) / 2.f;
  glm::vec3 extent = (max - min) / 2.f;
  return {center, extent};
}

SceneManager::ProcessedMeshes SceneManager::processMeshesBaked(const tinygltf::Model& model) const
{
  // NOTE: glTF assets can have pretty wonky data layouts which are not appropriate
  // for real-time rendering, so we have to press the data first. In serious engines
  // this is mitigated by storing assets on the disc in an engine-specific format that
  // is appropriate for GPU upload right after reading from disc.

  ProcessedMeshes result;

  // Pre-allocate enough memory so as not to hit the
  // allocator on the memcpy hotpath
  {
    std::size_t vertexBytes = 0;
    std::size_t vertexOffset = 0;
    std::size_t indexBytes = 0;

    for (const auto& bufView : model.bufferViews)
    {
      switch (bufView.target)
      {
      case TINYGLTF_TARGET_ARRAY_BUFFER:
        // We search for last buffer view
        vertexBytes = bufView.byteLength;
        vertexOffset = bufView.byteOffset;
        break;
      case TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER:
        // We search for last buffer view
        indexBytes = bufView.byteLength;
        break;
      default:
        break;
      }
    }
    result.vertices.resize(vertexBytes / sizeof(Vertex));
    result.indices.resize(indexBytes / sizeof(std::uint32_t));
    std::memcpy(result.indices.data() , model.buffers.back().data.data(), indexBytes);
    std::memcpy(result.vertices.data(), model.buffers.back().data.data() + vertexOffset, vertexBytes);
  }
  

  {
    std::size_t totalPrimitives = 0;
    for (const auto& mesh : model.meshes)
      totalPrimitives += mesh.primitives.size();
    result.relems.reserve(totalPrimitives);
    result.bounds.reserve(totalPrimitives);
  }
  result.meshes.reserve(model.meshes.size());

  size_t idxOffset = 0;
  size_t vrtOffset = 0;
  for (const auto& mesh : model.meshes)
  {
    result.meshes.push_back(Mesh{
      .firstRelem = static_cast<std::uint32_t>(result.relems.size()),
      .relemCount = static_cast<std::uint32_t>(mesh.primitives.size()),
    });

    for (const auto& prim : mesh.primitives)
    {
      if (prim.mode != TINYGLTF_MODE_TRIANGLES)
      {
        spdlog::warn(
          "Encountered a non-triangles primitive, these are not supported for now, skipping it!");
        --result.meshes.back().relemCount;
        continue;
      }
      std::size_t vrtCount = model.accessors[prim.attributes.at("POSITION")].count;
      result.relems.push_back(RenderElement{
        .vertexOffset = static_cast<std::uint32_t>(vrtOffset),
        .indexOffset = static_cast<std::uint32_t>(idxOffset),
        .indexCount = static_cast<std::uint32_t>(model.accessors[prim.indices].count),
        .materialId = static_cast<Material::Id>(prim.material),
      });
      result.bounds.emplace_back(getBounds(std::span(result.vertices).subspan(vrtOffset, vrtCount)));

      idxOffset += model.accessors[prim.indices].count;
      vrtOffset += vrtCount;
    }
  }

  return result;
}

SceneManager::ProcessedMeshes SceneManager::processMeshes(const tinygltf::Model& model) const
{
  // NOTE: glTF assets can have pretty wonky data layouts which are not appropriate
  // for real-time rendering, so we have to press the data first. In serious engines
  // this is mitigated by storing assets on the disc in an engine-specific format that
  // is appropriate for GPU upload right after reading from disc.

  ProcessedMeshes result;

  // Pre-allocate enough memory so as not to hit the
  // allocator on the memcpy hotpath
  {
    std::size_t vertexBytes = 0;
    std::size_t indexBytes = 0;
    for (const auto& bufView : model.bufferViews)
    {
      switch (bufView.target)
      {
      case TINYGLTF_TARGET_ARRAY_BUFFER:
        vertexBytes += bufView.byteLength;
        break;
      case TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER:
        indexBytes += bufView.byteLength;
        break;
      default:
        break;
      }
    }
    result.vertices.reserve(vertexBytes / sizeof(Vertex));
    result.indices.reserve(indexBytes / sizeof(std::uint32_t));
  }

  {
    std::size_t totalPrimitives = 0;
    for (const auto& mesh : model.meshes)
      totalPrimitives += mesh.primitives.size();
    result.relems.reserve(totalPrimitives);
    result.bounds.reserve(totalPrimitives);
  }

  result.meshes.reserve(model.meshes.size());

  for (const auto& mesh : model.meshes)
  {
    result.meshes.push_back(Mesh{
      .firstRelem = static_cast<std::uint32_t>(result.relems.size()),
      .relemCount = static_cast<std::uint32_t>(mesh.primitives.size()),
    });

    for (const auto& prim : mesh.primitives)
    {
      if (prim.mode != TINYGLTF_MODE_TRIANGLES)
      {
        spdlog::warn(
          "Encountered a non-triangles primitive, these are not supported for now, skipping it!");
        --result.meshes.back().relemCount;
        continue;
      }

      const auto normalIt = prim.attributes.find("NORMAL");
      const auto tangentIt = prim.attributes.find("TANGENT");
      const auto texcoordIt = prim.attributes.find("TEXCOORD_0");
      const auto normTexcoordIt = prim.attributes.find("TEXCOORD_1");

      const bool hasNormals = normalIt != prim.attributes.end();
      const bool hasTangents = tangentIt != prim.attributes.end();
      const bool hasTexcoord = texcoordIt != prim.attributes.end();
      const bool hasNormTexcoord = normTexcoordIt != prim.attributes.end();
      std::array accessorIndices{
        prim.indices,
        prim.attributes.at("POSITION"),
        hasNormals ? normalIt->second : -1,
        hasTangents ? tangentIt->second : -1,
        hasTexcoord ? texcoordIt->second : -1,
        hasNormTexcoord ? normTexcoordIt->second : -1,
      };

      std::array accessors{
        &model.accessors[prim.indices],
        &model.accessors[accessorIndices[1]],
        hasNormals ? &model.accessors[accessorIndices[2]] : nullptr,
        hasTangents ? &model.accessors[accessorIndices[3]] : nullptr,
        hasTexcoord ? &model.accessors[accessorIndices[4]] : nullptr,
        hasNormTexcoord ? &model.accessors[accessorIndices[5]] : nullptr,
      };

      std::array bufViews{
        &model.bufferViews[accessors[0]->bufferView],
        &model.bufferViews[accessors[1]->bufferView],
        hasNormals ? &model.bufferViews[accessors[2]->bufferView] : nullptr,
        hasTangents ? &model.bufferViews[accessors[3]->bufferView] : nullptr,
        hasTexcoord ? &model.bufferViews[accessors[4]->bufferView] : nullptr,
        hasNormTexcoord ? &model.bufferViews[accessors[5]->bufferView] : nullptr,
      };

      result.relems.push_back(RenderElement{
        .vertexOffset = static_cast<std::uint32_t>(result.vertices.size()),
        .indexOffset = static_cast<std::uint32_t>(result.indices.size()),
        .indexCount = static_cast<std::uint32_t>(accessors[0]->count),
        .materialId = static_cast<Material::Id>(prim.material),
      }); 

      const std::size_t vertexCount = accessors[1]->count;

      std::array ptrs{
        reinterpret_cast<const std::byte*>(model.buffers[bufViews[0]->buffer].data.data()) +
          bufViews[0]->byteOffset + accessors[0]->byteOffset,
        reinterpret_cast<const std::byte*>(model.buffers[bufViews[1]->buffer].data.data()) +
          bufViews[1]->byteOffset + accessors[1]->byteOffset,
        hasNormals
          ? reinterpret_cast<const std::byte*>(model.buffers[bufViews[2]->buffer].data.data()) +
            bufViews[2]->byteOffset + accessors[2]->byteOffset
          : nullptr,
        hasTangents
          ? reinterpret_cast<const std::byte*>(model.buffers[bufViews[3]->buffer].data.data()) +
            bufViews[3]->byteOffset + accessors[3]->byteOffset
          : nullptr,
        hasTexcoord
          ? reinterpret_cast<const std::byte*>(model.buffers[bufViews[4]->buffer].data.data()) +
            bufViews[4]->byteOffset + accessors[4]->byteOffset
          : nullptr,
        hasNormTexcoord
          ? reinterpret_cast<const std::byte*>(model.buffers[bufViews[5]->buffer].data.data()) +
            bufViews[4]->byteOffset + accessors[5]->byteOffset
          : nullptr,
      };

      std::array strides{
        bufViews[0]->byteStride != 0
          ? bufViews[0]->byteStride
          : tinygltf::GetComponentSizeInBytes(accessors[0]->componentType) *
            tinygltf::GetNumComponentsInType(accessors[0]->type),
        bufViews[1]->byteStride != 0
          ? bufViews[1]->byteStride
          : tinygltf::GetComponentSizeInBytes(accessors[1]->componentType) *
            tinygltf::GetNumComponentsInType(accessors[1]->type),
        hasNormals ? (bufViews[2]->byteStride != 0
                        ? bufViews[2]->byteStride
                        : tinygltf::GetComponentSizeInBytes(accessors[2]->componentType) *
                          tinygltf::GetNumComponentsInType(accessors[2]->type))
                   : 0,
        hasTangents ? (bufViews[3]->byteStride != 0
                         ? bufViews[3]->byteStride
                         : tinygltf::GetComponentSizeInBytes(accessors[3]->componentType) *
                           tinygltf::GetNumComponentsInType(accessors[3]->type))
                    : 0,
        hasTexcoord ? (bufViews[4]->byteStride != 0
                         ? bufViews[4]->byteStride
                         : tinygltf::GetComponentSizeInBytes(accessors[4]->componentType) *
                           tinygltf::GetNumComponentsInType(accessors[4]->type))
                    : 0,
        hasNormTexcoord ? (bufViews[5]->byteStride != 0
              ? bufViews[5]->byteStride
              : tinygltf::GetComponentSizeInBytes(accessors[5]->componentType) *
                tinygltf::GetNumComponentsInType(accessors[5]->type))
                    : 0,
      };

      for (std::size_t i = 0; i < vertexCount; ++i)
      {
        auto& vtx = result.vertices.emplace_back();
        glm::vec3 pos;
        // Fall back to 0 in case we don't have something.
        // NOTE: if tangents are not available, one could use http://mikktspace.com/
        // NOTE: if normals are not available, reconstructing them is possible but will look ugly
        glm::vec3 normal{0};
        glm::vec3 tangent{0};
        glm::vec2 texcoord{0};
        glm::vec2 normTexcoord{0};
        std::memcpy(&pos, ptrs[1], sizeof(pos));

        // NOTE: it's faster to do a template here with specializations for all combinations than to
        // do ifs at runtime. Also, SIMD should be used. Try implementing this!
        if (hasNormals)
          std::memcpy(&normal, ptrs[2], sizeof(normal));
        if (hasTangents)
          std::memcpy(&tangent, ptrs[3], sizeof(tangent));
        if (hasTexcoord)
          std::memcpy(&texcoord, ptrs[4], sizeof(texcoord));
        if (hasNormTexcoord)
          std::memcpy(&normTexcoord, ptrs[5], sizeof(texcoord));

        vtx.positionAndNormal = glm::vec4(pos, std::bit_cast<float>(encode_normal(normal)));
        vtx.texCoordAndTangentAndPadding =
          glm::vec4(texcoord, std::bit_cast<float>(encode_normal(tangent)), 0);
        vtx.normTexCoord = glm::vec4(normTexcoord, 0, 0);
        ptrs[1] += strides[1];
        if (hasNormals)
          ptrs[2] += strides[2];
        if (hasTangents)
          ptrs[3] += strides[3];
        if (hasTexcoord)
          ptrs[4] += strides[4];
        if (hasNormTexcoord)
          ptrs[5] += strides[5];
      }

      // Indices are guaranteed to have no stride
      ETNA_VERIFY(bufViews[0]->byteStride == 0);
      const std::size_t indexCount = accessors[0]->count;
      if (accessors[0]->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
      {
        for (std::size_t i = 0; i < indexCount; ++i)
        {
          std::uint16_t index;
          std::memcpy(&index, ptrs[0], sizeof(index));
          result.indices.push_back(index);
          ptrs[0] += 2;
        }
      }
      else if (accessors[0]->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
      {
        const std::size_t lastTotalIndices = result.indices.size();
        result.indices.resize(lastTotalIndices + indexCount);
        std::memcpy(
          result.indices.data() + lastTotalIndices,
          ptrs[0],
          sizeof(result.indices[0]) * indexCount);
      }
      result.bounds.emplace_back(getBounds(std::span(result.vertices).subspan(result.relems.back().vertexOffset, vertexCount)));
    }
  }

  return result;
}

void SceneManager::uploadData(
  std::span<const Vertex> vertices, std::span<const std::uint32_t> indices)
{
  unifiedVbuf = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
    .size = vertices.size_bytes(),
    .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "unifiedVbuf",
  });

  unifiedIbuf = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
    .size = indices.size_bytes(),
    .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "unifiedIbuf",
  });

  transferHelper.uploadBuffer<Vertex>(*oneShotCommands, unifiedVbuf, 0, vertices);
  transferHelper.uploadBuffer<std::uint32_t>(*oneShotCommands, unifiedIbuf, 0, indices);
}

void SceneManager::selectScene(std::filesystem::path path)
{
  auto maybeModel = loadModel(path);
  if (!maybeModel.has_value())
    return;

  auto model = std::move(*maybeModel);
  loadModelResources(path.parent_path(), model);

  processMaterials(model);
  // By aggregating all SceneManager fields mutations here,
  // we guarantee that we don't forget to clear something
  // when re-loading a scene.

  // NOTE: you might want to store these on the GPU for GPU-driven rendering.
  auto [instMats, instMeshes] = processInstances(model);
  instanceMatrices = std::move(instMats);
  instanceMeshes = std::move(instMeshes);

  auto [verts, inds, relems, meshs, bbs] = processMeshes(model);

  renderElements = std::move(relems);
  meshes = std::move(meshs);
  bounds = std::move(bbs);


  setupLights();
  uploadData(verts, inds);
}

void SceneManager::selectSceneBaked(std::filesystem::path path)
{
  auto maybeModel = loadModel(path);
  if (!maybeModel.has_value())
    return;

  auto model = std::move(*maybeModel);
  loadModelResources(path.parent_path(), model);

  processMaterials(model);
  // By aggregating all SceneManager fields mutations here,
  // we guarantee that we don't forget to clear something
  // when re-loading a scene.

  // NOTE: you might want to store these on the GPU for GPU-driven rendering.
  auto [instMats, instMeshes] = processInstances(model);
  instanceMatrices = std::move(instMats);
  instanceMeshes = std::move(instMeshes);

  auto [verts, inds, relems, meshs, bbs] = processMeshesBaked(model);

  renderElements = std::move(relems);
  meshes = std::move(meshs);
  bounds = std::move(bbs);

  setupLights();
  uploadData(verts, inds);
}

etna::VertexByteStreamFormatDescription SceneManager::getVertexFormatDescription()
{
  return etna::VertexByteStreamFormatDescription{
    .stride = sizeof(Vertex),
    .attributes = {
      etna::VertexByteStreamFormatDescription::Attribute{
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = 0,
      },
      etna::VertexByteStreamFormatDescription::Attribute{
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = sizeof(glm::vec4),
      },
      etna::VertexByteStreamFormatDescription::Attribute{
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = 2 * sizeof(glm::vec4),
      },
    }};
}

static glm::vec4 randomColor() {
  return glm::vec4{
    rand() / static_cast<float>(RAND_MAX),
    rand() / static_cast<float>(RAND_MAX),
    rand() / static_cast<float>(RAND_MAX),
    1.f,
  };
}

void SceneManager::setupLights()
{

  lightSources.add(/* 0*/  {{0.f, 40.f, 0.f,  0.f}, glm::vec4(0.6, 0.6, 0.6, 1.), 0.f});
#if 0
  const float lampRange = 2.5f;
  const glm::vec4 lampColor = glm::vec4(0.8, 0.8, 0, 1);
  const float bugRange = 0.4f;
  
  lightSources.add(/* 1*/  {{  -2.5630727f,  1.4107137f,   6.890522f, lampRange}, lampColor, .05f});
  lightSources.add(/* 2*/  {{   2.4905527f,  1.4413857f,   1.512132f, lampRange}, lampColor, .05f});
  lightSources.add(/* 3*/  {{  -0.7068754f,  1.4591746f,  -2.777065f, lampRange}, lampColor, .05f});
  lightSources.add(/* 4*/  {{  -0.6854431f,  1.5465388f,  -6.792568f, lampRange}, lampColor, .05f});
  lightSources.add(/* 5*/  {{   2.4870062f,  1.4218036f,  -6.782385f, lampRange}, lampColor, .05f});
  lightSources.add(/* 6*/  {{   2.4570112f,  1.5047158f,  -2.755790f, lampRange}, lampColor, .05f});
  lightSources.add(/* 7*/  {{   2.4768906f,  1.4559691f,   1.476415f, lampRange}, lampColor, .05f});
  lightSources.add(/* 8*/  {{  -2.2359478f,  1.4744445f,   1.737469f, lampRange}, lampColor, .05f});
  lightSources.add(/* 9*/  {{   2.8261878f,  1.3479835f,   1.977847f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*10*/  {{   2.6073039f,  1.5171486f,   1.263106f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*11*/  {{  -4.0668960f, -1.6299276f,  -8.455827f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*12*/  {{  -7.8137620f, -0.8250914f, -10.178850f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*13*/  {{ -15.5644180f,  2.1619153f,  -5.488131f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*14*/  {{ -18.3982730f,  0.3639415f,  -1.232591f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*15*/  {{ -13.8580230f, -1.9076519f,   9.474797f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*16*/  {{  -4.9035020f, -1.1138389f,   9.104493f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*17*/  {{  15.1646660f, -5.8738947f,  10.840951f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*18*/  {{  15.5161990f, -5.8145804f,   9.848208f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*19*/  {{  15.7284720f, -5.7917670f,   9.248762f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*20*/  {{  15.0396520f, -5.7861010f,   7.578271f,  bugRange}, randomColor(), bugRange / 10});
  lightSources.add(/*21*/  {{  14.9165880f, -5.6621490f,   6.895270f,  bugRange}, randomColor(), bugRange / 10});
#endif 
  for (size_t i = 0; i < 1000; ++i) {
    auto randCoord = []() {return (rand() % 400 - 200) / 10.f;};
    auto id = lightSources.emplace(glm::vec4{randCoord(), rand() % 10 - 2, randCoord(), 1.f}, randomColor(), 0.1f, 2.f * randomColor(), randomColor());
    lightSources.get(id).floatingAmplitude.w *= lightSources[id].visibleRadius / 10;
  }
}

void SceneManager::loadModelResources(std::filesystem::path path, const tinygltf::Model& model)
{
  //! Assume source is always equals texture id.
  //TODO: Implement properly
  auto& ctx = etna::get_context();
  for (auto tex : model.images) {
    int width, height, nChans;
    auto filepath = path / tex.uri;
    auto* imageBytes = stbi_load(filepath.generic_string<char>().c_str(), &width, &height, &nChans, STBI_rgb_alpha);
    if (imageBytes == nullptr)
    {
      spdlog::log(spdlog::level::err, "Image \"{}\" load is unsuccessful", tex.uri);
      return;
    }
    size_t size = static_cast<std::size_t>(width * height * 4);
    
    auto buf = ctx.createBuffer({
        .size = static_cast<vk::DeviceSize>(size),
        .bufferUsage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
        .name = "tmp load buf",
    });

    etna::BlockingTransferHelper transferHelper({
        .stagingSize = size,
    });

    auto cmdMgr = ctx.createOneShotCmdMgr();
    transferHelper.uploadBuffer(
    *cmdMgr, buf, 0, std::span<const std::byte>((std::byte*)imageBytes, size));

    auto img = ctx.createImage({
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
        .name = tex.uri,
        .format = vk::Format::eR8G8B8A8Unorm,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
    });

    auto cmdBuf = cmdMgr->start();
    ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
        etna::set_state(
            cmdBuf,
            img.get(),
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferWrite,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageAspectFlagBits::eColor
        );
        etna::flush_barriers(cmdBuf);


        vk::BufferImageCopy bic[1]{};
        bic[0].setImageExtent({static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1});
        bic[0].setImageOffset({});
        bic[0].setImageSubresource({
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .layerCount = 1,
        });


        cmdBuf.copyBufferToImage(buf.get(), img.get(), vk::ImageLayout::eTransferDstOptimal, bic);

        etna::set_state(
            cmdBuf,
            img.get(),
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::AccessFlagBits2::eShaderRead,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::ImageAspectFlagBits::eColor
        );

        etna::flush_barriers(cmdBuf);

    }
    ETNA_CHECK_VK_RESULT(cmdBuf.end());
    cmdMgr->submitAndWait(std::move(cmdBuf));
    spdlog::info("New texture: {} {{", textures.size());
    spdlog::info("    .name={}", tex.uri);
    spdlog::info("}}");
    textures.emplace(std::move(img));
  }
}

void SceneManager::processMaterials(const tinygltf::Model& model) {

  for(auto material : model.materials) {
    Material m;
    if (material.pbrMetallicRoughness.baseColorFactor.size() == 4) {
      m.baseColor = glm::vec4{
        material.pbrMetallicRoughness.baseColorFactor[0],
        material.pbrMetallicRoughness.baseColorFactor[1],
        material.pbrMetallicRoughness.baseColorFactor[2],
        material.pbrMetallicRoughness.baseColorFactor[3]
      };
    } else {
      m.baseColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
    }

    m.EMR_Factor.g = material.pbrMetallicRoughness.roughnessFactor;
    m.EMR_Factor.b = material.pbrMetallicRoughness.metallicFactor;

    if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
      m.baseColorTexture = static_cast<decltype(m.baseColorTexture)>(material.pbrMetallicRoughness.baseColorTexture.index);
    } else {
      m.baseColorTexture = getStubTexture();
    }

    if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
      m.metallicRoughnessTexture = static_cast<decltype(m.metallicRoughnessTexture)>(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
    } else {
      m.metallicRoughnessTexture = getStubTexture();
    }

    if (material.normalTexture.index != -1) {
      m.normalTexture = static_cast<decltype(m.normalTexture)>(material.normalTexture.index);
    } else {
      m.normalTexture = getStubBlueTexture();
    }
    spdlog::info("New material: {} {{", materials.size());
    spdlog::info("    .baseColorTexture={}", static_cast<uint32_t>(m.baseColorTexture));
    spdlog::info("}");

    materials.add(std::move(m));
  }
}

Texture::Id SceneManager::getStubTexture() {
  if (stubTexture != Texture::Id::Invalid) {
    return stubTexture;
  }
  etna::Image tex = etna::get_context().createImage({
    .extent = {1, 1, 1},
    .name = "stub",
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
    .type = vk::ImageType::e2D,
  });

  auto cmdMgr = etna::get_context().createOneShotCmdMgr();
  auto cmdBuf = cmdMgr->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  {
    etna::RenderTargetState renderTargets(
      cmdBuf,
      {{0, 0}, {1, 1}},
      {{.image=tex.get(), .view=tex.getView({}), .clearColorValue={1.f, 1.f, 1.f, 1.f}}},
      {}
    );
  }
  etna::set_state(
    cmdBuf, 
    tex.get(), 
    vk::PipelineStageFlagBits2::eAllCommands, 
    vk::AccessFlagBits2::eShaderSampledRead, 
    vk::ImageLayout::eShaderReadOnlyOptimal, 
    vk::ImageAspectFlagBits::eColor
  );
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdMgr->submitAndWait(cmdBuf);
  return stubTexture = textures.emplace(std::move(tex));
}

Texture::Id SceneManager::getStubRedTexture() {
  if (stubRedTexture != Texture::Id::Invalid) {
    return stubRedTexture;
  }
  etna::Image tex = etna::get_context().createImage({
    .extent = {1, 1, 1},
    .name = "stub red",
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
    .type = vk::ImageType::e2D,
  });

  auto cmdMgr = etna::get_context().createOneShotCmdMgr();
  auto cmdBuf = cmdMgr->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  {
    etna::RenderTargetState renderTargets(
      cmdBuf,
      {{0, 0}, {1, 1}},
      {{.image=tex.get(), .view=tex.getView({}), .clearColorValue={1.f, 0.f, 0.f, 0.f}}},
      {}
    );
  }
  etna::set_state(
    cmdBuf, 
    tex.get(), 
    vk::PipelineStageFlagBits2::eAllCommands, 
    vk::AccessFlagBits2::eShaderSampledRead, 
    vk::ImageLayout::eShaderReadOnlyOptimal, 
    vk::ImageAspectFlagBits::eColor
  );
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdMgr->submitAndWait(cmdBuf);
  return stubRedTexture = textures.emplace(std::move(tex));
}

Texture::Id SceneManager::getStubBlueTexture() {
  if (stubBlueTexture != Texture::Id::Invalid) {
    return stubBlueTexture;
  }
  etna::Image tex = etna::get_context().createImage({
    .extent = {1, 1, 1},
    .name = "stub blue",
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
    .type = vk::ImageType::e2D,
  });

  auto cmdMgr = etna::get_context().createOneShotCmdMgr();
  auto cmdBuf = cmdMgr->start();
  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  {
    etna::RenderTargetState renderTargets(
      cmdBuf,
      {{0, 0}, {1, 1}},
      {{.image=tex.get(), .view=tex.getView({}), .clearColorValue={0.f, 0.f, 1.f, 0.f}}},
      {}
    );
  }
  etna::set_state(
    cmdBuf, 
    tex.get(), 
    vk::PipelineStageFlagBits2::eAllCommands, 
    vk::AccessFlagBits2::eShaderSampledRead, 
    vk::ImageLayout::eShaderReadOnlyOptimal, 
    vk::ImageAspectFlagBits::eColor
  );
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmdMgr->submitAndWait(cmdBuf);
  return stubBlueTexture = textures.emplace(std::move(tex));
}

Material::Id SceneManager::getStubMaterial() {
  if (stubMaterial != Material::Id::Invalid) {
    return stubMaterial;
  }

  Material stub = {
    .baseColorTexture = getStubTexture(),
    .metallicRoughnessTexture = getStubTexture(),
    .emissiveFactorTexture = getStubTexture(),
  };

  return stubMaterial = materials.add(stub);
}
