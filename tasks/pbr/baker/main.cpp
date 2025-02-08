
#include "VertexAttributes.hpp"
#include <string_view>
#include <spdlog/spdlog.h>
#include <fmt/std.h>
#include <tiny_gltf.h>
#include <filesystem>

using Path = std::filesystem::path;

struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;
};

// A mesh is a collection of relems. A scene may have the same mesh
// located in several different places, so a scene consists of **instances**,
// not meshes.
struct Mesh
{
  std::uint32_t firstRelem;
  std::uint32_t relemCount;
};

struct BakeResult
{
  std::vector<VertexAttrs> vertices;
  std::vector<uint32_t> indices;
  std::vector<RenderElement> relems;
  std::vector<Mesh> meshes;
};

std::optional<tinygltf::Model> load_model(std::filesystem::path path)
{
  tinygltf::TinyGLTF loader;
  loader.SetImagesAsIs(true);
  tinygltf::Model model;

  std::string error;
  std::string warning;
  bool success = false;

  auto ext = path.extension();
  if (ext == ".gltf")
    success = loader.LoadASCIIFromFile(&model, &error, &warning, path.generic_string<char>());
  else if (ext == ".glb")
    success = loader.LoadBinaryFromFile(&model, &error, &warning, path.generic_string<char>());
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

uint32_t compact_normal(glm::vec3 vec3)
{
  const std::int32_t x = static_cast<std::int32_t>(vec3.x * 32767.0f);
  const std::int32_t y = static_cast<std::int32_t>(vec3.y * 32767.0f);

  const std::uint32_t sign = vec3.z >= 0 ? 0 : 1;
  const std::uint32_t sx = static_cast<std::uint32_t>(x & 0xfffe) | sign;
  const std::uint32_t sy = static_cast<std::uint32_t>(y & 0xffff) << 16;

  return sx | sy;
}

BakeResult do_bakery(tinygltf::Model& model)
{
  BakeResult result;
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
    result.vertices.reserve(vertexBytes / sizeof(VertexAttrs));
    result.indices.reserve(indexBytes / sizeof(std::uint32_t));
  }

  {
    std::size_t totalPrimitives = 0;
    for (const auto& mesh : model.meshes)
      totalPrimitives += mesh.primitives.size();
    result.relems.reserve(totalPrimitives);
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
            bufViews[5]->byteOffset + accessors[5]->byteOffset
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
          std::memcpy(&normTexcoord, ptrs[5], sizeof(normTexcoord));


        vtx.cord = pos;
        vtx.norm = compact_normal(normal);
        vtx.texture = texcoord;
        vtx.tangent = compact_normal(tangent);
        vtx.normTexCoord = normTexcoord;

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
    }
  }

  return result;
}

void append_model(tinygltf::Model& model, BakeResult& baked, Path dest)
{
   for (auto& image: model.images) {
    if (image.uri.ends_with("jpeg")) {
      //Fix jpeg filename isn't supported 
      image.uri.pop_back();
      image.uri.back() = 'g';
    }
  }

  std::size_t indicesBytes =  baked.indices.size() * sizeof(uint32_t);
  std::size_t vertexBytes  =  baked.vertices.size() * sizeof(VertexAttrs);
  std::size_t vertOffset =  (indicesBytes + 31) & ~31ul; // Rounup;


  std::size_t bufId = model.buffers.size();
  
  {
    tinygltf::Buffer buffer{};
    buffer.name = dest.stem().generic_string<char>();
    buffer.uri = dest.filename().generic_string<char>();
    buffer.data.resize(vertOffset + vertexBytes);


    std::memcpy(buffer.data.data()             , baked.indices .data(), baked.indices .size() * sizeof(uint32_t   ));
    std::memcpy(buffer.data.data() + vertOffset, baked.vertices.data(), baked.vertices.size() * sizeof(VertexAttrs));
    model.buffers.push_back(buffer);
  }

  // std::size_t bvsId = model.bufferViews.size();
  {
    tinygltf::BufferView bvs[2] = {};
    bvs[0].name   = "indices_baked";
    bvs[0].buffer = bufId;
    bvs[0].byteOffset = 0;
    bvs[0].byteLength = baked.indices.size() * sizeof(uint32_t);
    bvs[0].byteStride = 0;
    bvs[0].target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    
    bvs[1].name   = "verticies_baked";
    bvs[1].buffer = bufId;
    bvs[1].byteOffset = vertOffset;
    bvs[1].byteLength = baked.vertices.size() * sizeof(VertexAttrs);
    bvs[1].byteStride = sizeof(VertexAttrs);
    bvs[1].target = TINYGLTF_TARGET_ARRAY_BUFFER;

    model.bufferViews.push_back(bvs[0]);
    model.bufferViews.push_back(bvs[1]);
  }
#if 0
  std::size_t acsId = model.accessors.size();
  {
    tinygltf::Accessor acss[5] = {};
    // Indicies accessor
    acss[0].bufferView = bvsId;
    acss[0].name = "indicies_baked";
    acss[0].byteOffset = 0;
    acss[0].normalized = false;
    acss[0].count = baked.indices.size();
    acss[0].type = TINYGLTF_TYPE_SCALAR;
    acss[0].componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;


    //Position
    acss[1].bufferView = bvsId + 1;
    acss[1].name = "indicies_baked";
    acss[1].byteOffset = offsetof(VertexAttrs, cord);
    acss[1].normalized = false;
    acss[1].count = baked.vertices.size();
    acss[1].type = TINYGLTF_TYPE_VEC3;
    acss[1].componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;

    //Normal
    acss[2].bufferView = bvsId + 1;
    acss[2].name = "normal_baked";
    acss[2].byteOffset = offsetof(VertexAttrs, norm);
    acss[2].normalized = true;
    acss[2].count = baked.vertices.size();
    acss[2].type = TINYGLTF_TYPE_VEC3;
    acss[2].componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

    //Tex cord
    acss[3].bufferView = bvsId + 1;
    acss[3].name = "texcoord_baked";
    acss[3].byteOffset = offsetof(VertexAttrs, texture);
    acss[3].normalized = false;
    acss[3].count = baked.vertices.size();
    acss[3].type = TINYGLTF_TYPE_VEC2;
    acss[3].componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;

    //Tangent
    acss[4].bufferView = bvsId + 1;
    acss[4].name = "tangent_baked";
    acss[4].byteOffset = offsetof(VertexAttrs, tangent);
    acss[4].normalized = true;
    acss[4].count = baked.vertices.size();
    acss[4].type = TINYGLTF_TYPE_VEC3;
    acss[4].componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

    model.accessors.push_back(acss[0]);
    model.accessors.push_back(acss[1]);
    model.accessors.push_back(acss[2]);
    model.accessors.push_back(acss[3]);
    model.accessors.push_back(acss[4]);
  }
  for (auto& mesh : model.meshes)
  {
    for (auto& prim : mesh.primitives)
    {
      if (prim.mode != TINYGLTF_MODE_TRIANGLES)
      {
        spdlog::warn(
          "Encountered a non-triangles primitive, these are not supported for now, skipping it!");
        continue;
      }

      const auto normalIt   = prim.attributes.find("NORMAL");
      const auto tangentIt  = prim.attributes.find("TANGENT");
      const auto texcoordIt = prim.attributes.find("TEXCOORD_0");
      const auto positionIt = prim.attributes.find("POSITION");
     
      const bool hasNormals = normalIt != prim.attributes.end();
      const bool hasTangents = tangentIt != prim.attributes.end();
      const bool hasTexcoord = texcoordIt != prim.attributes.end();
      const bool hasPosition = positionIt != prim.attributes.end();
      
      prim.indices = acsId + 0;
      if (hasPosition) positionIt->second =  acsId + 1;
      if (hasNormals ) normalIt  ->second =  acsId + 2;
      if (hasTexcoord) texcoordIt->second =  acsId + 3;  
      if (hasTangents) tangentIt ->second =  acsId + 4;
    }
  }
#endif
}

int bake(Path path)
{
  auto maybeModel = load_model(path);
  if (!maybeModel.has_value())
    return -1;
  auto model = std::move(*maybeModel);

  auto baked = do_bakery(model);

  Path dest = path;
  dest.replace_extension();
  dest += "_baked.bin";

  append_model(model, baked, dest);

  tinygltf::TinyGLTF storer;
  dest.replace_extension(".gltf");
  spdlog::info("Writing result to: {}", dest);
  if (!storer.WriteGltfSceneToFile(&model, dest.generic_string<char>(), false, false, true, false)) {
    spdlog::error("Writing is unsuccessful");
  }

  return 0;
}






int main(int argc, char* argv[])
{
  if(argc < 2) {
    spdlog::error("No filename provided");
    return 1;
  }

  return bake(argv[1]);
}
