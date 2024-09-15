#include "SceneLights.hpp"

#include <etna/GlobalContext.hpp>


std::optional<FinitePointLight> GltfLightTryPack<FinitePointLight>::tryPack(
  glm::mat4 transform, const tinygltf::Light& gltf_light)
{
  if (gltf_light.type != "point" || gltf_light.range == 0.0)
  {
    return std::nullopt;
  }

  glm::vec3 position = transform * glm::vec4(glm::vec3(0.0), 1.0);
  glm::vec3 color = glm::vec3(gltf_light.color[0], gltf_light.color[1], gltf_light.color[2]);

  return FinitePointLight{
    .positionRange = glm::vec4(position, gltf_light.range),
    .colorIntensity = glm::vec4(color, gltf_light.intensity),
  };
}

std::optional<InfinitePointLight> GltfLightTryPack<InfinitePointLight>::tryPack(
  glm::mat4 transform, const tinygltf::Light& gltf_light)
{
  if (gltf_light.type != "point" || gltf_light.range != 0.0)
  {
    return std::nullopt;
  }

  glm::vec3 position = transform * glm::vec4(glm::vec3(0.0), 1.0);
  glm::vec3 color = glm::vec3(gltf_light.color[0], gltf_light.color[1], gltf_light.color[2]);

  return InfinitePointLight{
    .position = glm::vec4(position, 0.0),
    .colorIntensity = glm::vec4(color, gltf_light.intensity),
  };
}

std::optional<DirectionalLight> GltfLightTryPack<DirectionalLight>::tryPack(
  glm::mat4 transform, const tinygltf::Light& gltf_light)
{
  if (gltf_light.type != "directional")
  {
    return std::nullopt;
  }

  glm::vec3 direction = glm::mat3(transform) * glm::vec3(0.0, 0.0, -1.0);
  glm::vec3 color = glm::vec3(gltf_light.color[0], gltf_light.color[1], gltf_light.color[2]);

  return DirectionalLight{
    .direction = glm::vec4(direction, 0.0),
    .colorIntensity = glm::vec4(color, gltf_light.intensity),
  };
}

std::optional<AmbientLight> GltfLightTryPack<AmbientLight>::tryPack(
  glm::mat4, const tinygltf::Light& gltf_light)
{
  if (gltf_light.type != "ambient")
  {
    return std::nullopt;
  }

  glm::vec3 color = glm::vec3(gltf_light.color[0], gltf_light.color[1], gltf_light.color[2]);

  return AmbientLight{
    .colorIntensity = glm::vec4(color, gltf_light.intensity),
  };
}

void SceneLights::load(
  std::span<const glm::mat4> instance_matrices,
  const tinygltf::Model& model,
  etna::BlockingTransferHelper& transfer_helper,
  etna::OneShotCmdMgr& one_shot_cmd_mgr)
{
  ForEachKnownLightType<LightBufferData>::Type data;

  for (std::size_t i = 0; i < model.nodes.size(); ++i)
  {
    if (model.nodes[i].light < 0)
    {
      continue;
    }

    const tinygltf::Light& gltfLight = model.lights[model.nodes[i].light];

    for_each_known_light_type(
      [&instance_matrices, &data, i, &gltfLight]<std::size_t I, typename L>() {
        std::optional<L> lightPacked =
          GltfLightTryPack<L>::tryPack(instance_matrices[i], gltfLight);
        if (lightPacked.has_value())
        {
          std::get<I>(data).push_back(*lightPacked);
        }
      });
  }

  for_each_known_light_type(
    [this, &transfer_helper, &one_shot_cmd_mgr, &data]<std::size_t I, typename L>() {
      if (std::get<I>(data).empty())
      {
        return;
      }

      auto& bufferData = std::get<I>(data);

      buffers[I].buffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
        .size = std::span(bufferData).size_bytes(),
        .bufferUsage =
          vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        .name = fmt::format("lights_buffer_{}", I),
      });

      transfer_helper.uploadBuffer<L>(one_shot_cmd_mgr, buffers[I].buffer, 0, bufferData);
      buffers[I].count = bufferData.size();
    });
}
