#include <filesystem>
#include <glm/glm.hpp>
#include "tiny_gltf.h"

// for some reason my local glm doesn't seem to get the glm::vec3 operator<
bool Vec3Comp(const glm::vec3 &vecA, const glm::vec3 &vecB)
{
 return vecA[0]<vecB[0]
        && vecA[1]<vecB[1]
        && vecA[2]<vecB[2];
}

struct Vertex
{
  glm::vec4 positionAndNormal;
  glm::vec4 texCoordAndTangentAndPadding;
};

struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;
};

struct Mesh
{
  std::uint32_t firstRelem;
  std::uint32_t relemCount;
};

struct ProcessedMeshes
{
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<RenderElement> relems;
  std::vector<Mesh> meshes;
};

std::uint32_t encode_normal(glm::vec3 normal)
{
  const std::uint8_t x = static_cast<std::uint8_t>(glm::round((normal.x) * 127.0f));
  const std::uint8_t y = static_cast<std::uint8_t>(glm::round((normal.y) * 127.0f));
  const std::uint8_t z = static_cast<std::uint8_t>(glm::round((normal.z) * 127.0f));

  const std::uint32_t sx = static_cast<std::uint32_t>(x);
  const std::uint32_t sy = static_cast<std::uint32_t>(y) << 8;
  const std::uint32_t sz = static_cast<std::uint32_t>(z) << 16;
  const std::uint32_t sw = static_cast<std::uint32_t>(127) << 24;

  return sx | sy | sz | sw;
}

std::optional<tinygltf::Model> loadModel(std::filesystem::path path)
{
  tinygltf::TinyGLTF loader;
  loader.SetImagesAsIs(true);
  tinygltf::Model model;

  std::string error;
  std::string warning;
  bool success = false;

  auto ext = path.extension();
  if (ext == ".gltf")
    success = loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
  else if (ext == ".glb")
    success = loader.LoadBinaryFromFile(&model, &error, &warning, path.string());
  else {
    return std::nullopt;
  }

  if (!success) {
    return std::nullopt;
  }

  return model;
}

// Copy from Scene Manager
ProcessedMeshes processMeshes(const tinygltf::Model& model)
{
  ProcessedMeshes result;

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
        --result.meshes.back().relemCount;
        continue;
      }

      const auto normalIt = prim.attributes.find("NORMAL");
      const auto tangentIt = prim.attributes.find("TANGENT");
      const auto texcoordIt = prim.attributes.find("TEXCOORD_0");

      const bool hasNormals = normalIt != prim.attributes.end();
      const bool hasTangents = tangentIt != prim.attributes.end();
      const bool hasTexcoord = texcoordIt != prim.attributes.end();
      std::array accessorIndices{
        prim.indices,
        prim.attributes.at("POSITION"),
        hasNormals ? normalIt->second : -1,
        hasTangents ? tangentIt->second : -1,
        hasTexcoord ? texcoordIt->second : -1,
      };

      std::array accessors{
        &model.accessors[prim.indices],
        &model.accessors[accessorIndices[1]],
        hasNormals ? &model.accessors[accessorIndices[2]] : nullptr,
        hasTangents ? &model.accessors[accessorIndices[3]] : nullptr,
        hasTexcoord ? &model.accessors[accessorIndices[4]] : nullptr,
      };

      std::array bufViews{
        &model.bufferViews[accessors[0]->bufferView],
        &model.bufferViews[accessors[1]->bufferView],
        hasNormals ? &model.bufferViews[accessors[2]->bufferView] : nullptr,
        hasTangents ? &model.bufferViews[accessors[3]->bufferView] : nullptr,
        hasTexcoord ? &model.bufferViews[accessors[4]->bufferView] : nullptr,
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
      };

      for (size_t i = 0; i < vertexCount; ++i)
      {
        auto& vtx = result.vertices.emplace_back();
        glm::vec3 pos;
        // Fall back to 0 in case we don't have something.
        // NOTE: if tangents are not available, one could use http://mikktspace.com/
        // NOTE: if normals are not available, reconstructing them is possible but will look ugly
        glm::vec3 normal{0};
        glm::vec3 tangent{0};
        glm::vec2 texcoord{0};
        std::memcpy(&pos, ptrs[1], sizeof(pos));

        // NOTE: it's faster to do a template here with specializations for all combinations than to
        // do ifs at runtime. Also, SIMD should be used. Try implementing this!
        if (hasNormals)
          std::memcpy(&normal, ptrs[2], sizeof(normal));
        if (hasTangents)
          std::memcpy(&tangent, ptrs[3], sizeof(tangent));
        if (hasTexcoord)
          std::memcpy(&texcoord, ptrs[4], sizeof(texcoord));


        vtx.positionAndNormal = glm::vec4(pos, std::bit_cast<float>(encode_normal(normal)));
        vtx.texCoordAndTangentAndPadding =
          glm::vec4(texcoord, std::bit_cast<float>(encode_normal(tangent)), 0);

        ptrs[1] += strides[1];
        if (hasNormals)
          ptrs[2] += strides[2];
        if (hasTangents)
          ptrs[3] += strides[3];
        if (hasTexcoord)
          ptrs[4] += strides[4];
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

int main(int argc, char* argv[])
{
  if (argc != 2) {
    return 1;
  }
  // ModelPaths
  std::filesystem::path modelPath(argv[1]);
  std::filesystem::path dir = modelPath.parent_path();
  std::filesystem::path name = modelPath.stem();
  std::filesystem::path bakedModelPath((dir / name).string().append("_baked.gltf"));
  std::filesystem::path bakedBinPath((std::move(dir) / std::move(name)).string().append("_baked.bin"));

  // Model work
  tinygltf::Model model = *loadModel(modelPath);
  model.extensionsRequired.push_back("KHR_mesh_quantization");
  model.extensionsUsed.push_back("KHR_mesh_quantization");

  auto [vertices, indices, relems, meshes] = processMeshes(model);
  
  std::size_t indiciesSize = indices.size() * sizeof(uint32_t);
  std::size_t verticesOffset = (indiciesSize + 15) / 16 * 16;
  std::size_t verticesSize = vertices.size() * sizeof(Vertex);
  
  // Buffer work
  tinygltf::Buffer tempBuffer;
  tempBuffer.name = bakedBinPath.stem().string();
  tempBuffer.uri = bakedBinPath.filename().string();
  tempBuffer.data.resize(verticesOffset + verticesSize);
  std::memcpy(tempBuffer.data.data(), indices.data(), indiciesSize);
  std::memcpy(tempBuffer.data.data() + verticesOffset, vertices.data(), verticesSize);
  model.buffers.clear();
  model.buffers.push_back(std::move(tempBuffer));

  //Buffer views
  model.bufferViews.clear();

  tinygltf::BufferView baked_indicies;
  baked_indicies.name = "baked_indicies";
  baked_indicies.buffer = 0;
  baked_indicies.byteOffset = 0;
  baked_indicies.byteLength = indiciesSize;
  baked_indicies.byteStride = 0;
  baked_indicies.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
  model.bufferViews.push_back(std::move(baked_indicies));

  tinygltf::BufferView baked_vertices;
  baked_vertices.name = "baked_vertices";
  baked_vertices.buffer = 0;
  baked_vertices.byteOffset = verticesOffset;
  baked_vertices.byteLength = verticesSize;
  baked_vertices.byteStride = sizeof(Vertex);
  baked_vertices.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
  model.bufferViews.push_back(std::move(baked_vertices));

  //Accessors
  std::vector<tinygltf::Accessor> accessors;
  for (size_t i = 0; i < model.meshes.size(); ++i) {
    auto& current_mesh = model.meshes[i];
    for (size_t j = 0; j < current_mesh.primitives.size(); ++j) {
      auto& cur_primitive = current_mesh.primitives[j];

      uint32_t max_index = 0;
      glm::vec3 max_values = glm::vec3(vertices[relems[meshes[i].firstRelem + j].vertexOffset].positionAndNormal);
      glm::vec3 min_values = max_values;
      for (size_t k = 0; k < relems[meshes[i].firstRelem + j].indexCount; ++k) {
        max_index = std::max(max_index, indices[k]); 

        glm::vec3 cur_vec = glm::vec3(vertices[relems[meshes[i].firstRelem + j].vertexOffset + k].positionAndNormal);
        if (Vec3Comp(max_values, cur_vec)) {
          max_values = cur_vec;
        }
        if (Vec3Comp(cur_vec, min_values)) {
          min_values = cur_vec;
        }
      }

      tinygltf::Accessor baked_indicies_accessor;
      baked_indicies_accessor.name = "baked_indicies_accessor";
      baked_indicies_accessor.bufferView = 0;
      baked_indicies_accessor.byteOffset = relems[meshes[i].firstRelem + j].indexOffset * sizeof(uint32_t);
      baked_indicies_accessor.count = relems[meshes[i].firstRelem + j].indexCount;
      baked_indicies_accessor.normalized = false;
      baked_indicies_accessor.type = TINYGLTF_TYPE_SCALAR;
      baked_indicies_accessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
      
      cur_primitive.indices = static_cast<int>(accessors.size());
      accessors.push_back(baked_indicies_accessor);

      bool normals = cur_primitive.attributes.find("NORMAL") != cur_primitive.attributes.end();
      bool texcoord = cur_primitive.attributes.find("TEXCOORD_0") != cur_primitive.attributes.end();
      bool tangent = cur_primitive.attributes.find("TANGENT") != cur_primitive.attributes.end();

      cur_primitive.attributes.clear();

      tinygltf::Accessor baked_position_accessor;
      baked_position_accessor.name = "baked_position_accessor";
      baked_position_accessor.bufferView = 1;
      baked_position_accessor.byteOffset = relems[meshes[i].firstRelem + j].vertexOffset * sizeof(Vertex);
      baked_position_accessor.count = max_index + 1;
      baked_position_accessor.normalized = false;
      baked_position_accessor.type = TINYGLTF_TYPE_VEC3;
      baked_position_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
      baked_position_accessor.maxValues = {max_values.x, max_values.y, max_values.z};
      baked_position_accessor.minValues = {min_values.x, min_values.y, min_values.z};

      cur_primitive.attributes.insert({"POSITION", static_cast<int>(accessors.size())});
      accessors.push_back(baked_position_accessor);

      if (normals) {
        tinygltf::Accessor baked_normal_accessor;
        baked_normal_accessor.name = "baked_normal_accessor";
        baked_normal_accessor.bufferView = 1;
        baked_normal_accessor.byteOffset = relems[meshes[i].firstRelem + j].vertexOffset * sizeof(Vertex) + 3 * sizeof(float);
        baked_normal_accessor.count = max_index + 1;
        baked_normal_accessor.normalized = true;
        baked_normal_accessor.type = TINYGLTF_TYPE_VEC3;
        baked_normal_accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

        cur_primitive.attributes.insert({"NORMAL", static_cast<int>(accessors.size())});
        accessors.push_back(baked_normal_accessor);
      }
      if (texcoord) {
        tinygltf::Accessor baked_texcoord_accessor;
        baked_texcoord_accessor.name = "baked_texCoord_accessor";
        baked_texcoord_accessor.bufferView = 1;
        baked_texcoord_accessor.byteOffset = relems[meshes[i].firstRelem + j].vertexOffset * sizeof(Vertex) * sizeof(Vertex) + 4 * sizeof(float);
        baked_texcoord_accessor.count = max_index + 1;
        baked_texcoord_accessor.normalized = false;
        baked_texcoord_accessor.type = TINYGLTF_TYPE_VEC2;
        baked_texcoord_accessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;

        cur_primitive.attributes.insert({"TEXCOORD_0", static_cast<int>(accessors.size())});
        accessors.push_back(baked_texcoord_accessor);
      }
      if (tangent) {
        tinygltf::Accessor baked_tangent_accessor;
        baked_tangent_accessor.name = "baked_tangent_accessor";
        baked_tangent_accessor.bufferView = 1;
        baked_tangent_accessor.byteOffset = relems[meshes[i].firstRelem + j].vertexOffset * sizeof(Vertex) + 6 * sizeof(float);
        baked_tangent_accessor.count = max_index + 1;
        baked_tangent_accessor.normalized = true;
        baked_tangent_accessor.type = TINYGLTF_TYPE_VEC4;
        baked_tangent_accessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;

        cur_primitive.attributes.insert({"TANGENT", static_cast<int>(accessors.size())});
        accessors.push_back(baked_tangent_accessor);
      }
    }
  }
  model.accessors = std::move(accessors);
  tinygltf::TinyGLTF saver;
  saver.SetImagesAsIs(true);
  bool success = false;

  success = saver.WriteGltfSceneToFile(&model, bakedModelPath.string(), 0, 0, 1, 0);
  if (!success)
  {
    return 1;
  }
}

