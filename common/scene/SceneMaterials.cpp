#include "SceneMaterials.hpp"

template <typename T, typename F>
  requires(std::is_invocable_r_v<std::uint8_t, F, T>)
static std::vector<glm::u8vec4> transformPixelsImpl(
  std::span<const T> data, std::size_t num_comp, F func)
{
  std::vector<glm::u8vec4> result(data.size() / num_comp, glm::u8vec4(0, 0, 0, 255));

  for (std::size_t i = 0; i < result.size(); ++i)
  {
    for (std::size_t j = 0; j < num_comp; ++j)
    {
      result[i][j] = func(data[i * num_comp + j]);
    }
  }

  return result;
}

template <std::unsigned_integral T>
static std::vector<glm::u8vec4> transformPixels(std::span<const T> data, std::size_t num_comp)
{
  return transformPixelsImpl(
    data, num_comp, [](T x) -> std::uint8_t { return (x >> ((sizeof(T) - 1) * 8)) & 0xFF; });
}

template <std::signed_integral T>
static std::vector<glm::u8vec4> transformPixels(std::span<const T> data, std::size_t num_comp)
{
  return transformPixelsImpl(data, num_comp, [](T x) -> std::uint8_t {
    return ((std::bit_cast<std::make_unsigned_t<T>>(x) << 1) >> ((sizeof(T) - 1) * 8)) & 0xFF;
  });
}

template <std::floating_point T>
static std::vector<glm::u8vec4> transformPixels(std::span<const T> data, int num_comp)
{
  return transformPixelsImpl(data, num_comp, [](T x) -> std::uint8_t { return x * 0xFF; });
}

template <typename T>
  requires(std::integral<T> || std::floating_point<T>)
static std::vector<glm::u8vec4> transformPixelsHelper(
  std::span<const unsigned char> data, int num_comp)
{
  return transformPixels(
    std::span(reinterpret_cast<const T*>(data.data()), data.size_bytes() / sizeof(T)), num_comp);
}

static std::vector<glm::u8vec4> transformPixels(
  std::span<const unsigned char> data, int pixel_type, int num_comp)
{
  switch (pixel_type)
  {
  case TINYGLTF_COMPONENT_TYPE_BYTE:
    return transformPixelsHelper<std::int8_t>(data, num_comp);
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
    return transformPixelsHelper<std::uint8_t>(data, num_comp);
  case TINYGLTF_COMPONENT_TYPE_SHORT:
    return transformPixelsHelper<std::int16_t>(data, num_comp);
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
    return transformPixelsHelper<std::uint16_t>(data, num_comp);
  case TINYGLTF_COMPONENT_TYPE_INT:
    return transformPixelsHelper<std::int32_t>(data, num_comp);
  case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    return transformPixelsHelper<std::uint32_t>(data, num_comp);
  case TINYGLTF_COMPONENT_TYPE_FLOAT:
    return transformPixelsHelper<float>(data, num_comp);
  case TINYGLTF_COMPONENT_TYPE_DOUBLE:
    return transformPixelsHelper<double>(data, num_comp);
  default:
    ETNA_PANIC("Unknown pixel type: {}", pixel_type);
  }
}

static vk::Filter convertFilter(int filter)
{
  switch (filter)
  {
  case TINYGLTF_TEXTURE_FILTER_LINEAR:
    return vk::Filter::eLinear;
  case TINYGLTF_TEXTURE_FILTER_NEAREST:
    return vk::Filter::eNearest;
  default:
    return vk::Filter::eLinear;
  }
}

static vk::SamplerAddressMode convertSamplerAddressMode(int mode)
{
  switch (mode)
  {
  case TINYGLTF_TEXTURE_WRAP_REPEAT:
    return vk::SamplerAddressMode::eRepeat;
  case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
    return vk::SamplerAddressMode::eClampToEdge;
  case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
    return vk::SamplerAddressMode::eMirroredRepeat;
  default:
    ETNA_PANIC("Unknown sampler address mode: {}", mode);
  }
}

void SceneMaterials::load(const tinygltf::Model& model, etna::OneShotCmdMgr& one_shot_cmd_mgr)
{
  vk::CommandBuffer commandBuffer = one_shot_cmd_mgr.start();

  images.clear();

  for (const auto& image : model.images)
  {
    std::vector<glm::u8vec4> data = transformPixels(image.image, image.pixel_type, image.component);

    images.push_back(etna::create_image_from_bytes(
      etna::Image::CreateInfo{
        .extent =
          vk::Extent3D{
            .width = static_cast<std::uint32_t>(image.width),
            .height = static_cast<std::uint32_t>(image.height),
            .depth = 1,
          },
        .name = fmt::format("image_{}", image.name),
        .format = vk::Format::eR8G8B8A8Unorm,
        .imageUsage = vk::ImageUsageFlagBits::eSampled,
      },
      commandBuffer,
      data.data()));
  }

  samplers.clear();

  for (const auto& sampler : model.samplers)
  {
    samplers.push_back(etna::Sampler(etna::Sampler::CreateInfo{
      .filter = convertFilter(sampler.minFilter),
      .addressMode = convertSamplerAddressMode(sampler.wrapS),
      .name = fmt::format("sampler_{}", sampler.name),
    }));
  }

  textures.clear();

  for (const auto& texture : model.textures)
  {
    textures.push_back(Texture{
      .image = static_cast<std::size_t>(texture.source),
      .sampler = static_cast<std::size_t>(texture.sampler),
    });
  }

  materials.clear();

  for (const auto& gltfMaterial : model.materials)
  {
    Material material;

    const tinygltf::PbrMetallicRoughness& pbr = gltfMaterial.pbrMetallicRoughness;

    if (pbr.baseColorTexture.index != -1)
    {
      material.baseColorTexture = pbr.baseColorTexture.index;
    }
    material.baseColor = glm::vec4(
      pbr.baseColorFactor[0],
      pbr.baseColorFactor[1],
      pbr.baseColorFactor[2],
      pbr.baseColorFactor[3]);

    if (pbr.metallicRoughnessTexture.index != -1)
    {
      material.metallicRoughnessTexture = pbr.metallicRoughnessTexture.index;
    }
    material.metallicFactor = pbr.metallicFactor;
    material.roughnessFactor = pbr.roughnessFactor;

    if (gltfMaterial.normalTexture.index != -1)
    {
      material.normalTexture = gltfMaterial.normalTexture.index;
    }
    if (gltfMaterial.occlusionTexture.index != -1)
    {
      material.occlusionTexture = gltfMaterial.occlusionTexture.index;
    }

    if (gltfMaterial.emissiveTexture.index != -1)
    {
      material.emissiveTexture = gltfMaterial.emissiveTexture.index;
    }
    material.emissiveFactor = glm::vec4(
      gltfMaterial.emissiveFactor[0],
      gltfMaterial.emissiveFactor[1],
      gltfMaterial.emissiveFactor[2],
      gltfMaterial.emissiveFactor[3]);

    materials.push_back(material);
  }
}

const etna::Image& SceneMaterials::getImage(std::size_t image_index) const
{
  ETNA_VERIFYF(
    image_index < images.size(),
    "The image index ({}) is greater than or equal to the number of images ({})",
    image_index,
    images.size());
  return images[image_index];
}

const etna::Sampler& SceneMaterials::getSampler(std::size_t sampler_index) const
{
  ETNA_VERIFYF(
    sampler_index < samplers.size(),
    "The sampler index ({}) is greater than or equal to the number of samplers ({})",
    sampler_index,
    samplers.size());
  return samplers[sampler_index];
}

const Texture& SceneMaterials::getTexture(std::size_t texture_index) const
{
  ETNA_VERIFYF(
    texture_index < textures.size(),
    "The texture index ({}) is greater than or equal to the number of textures ({})",
    texture_index,
    textures.size());
  return textures[texture_index];
}

const Material& SceneMaterials::getMaterial(std::size_t material_index) const
{
  ETNA_VERIFYF(
    material_index < materials.size(),
    "The material index ({}) is greater than or equal to the number of materials ({})",
    material_index,
    materials.size());
  return materials[material_index];
}
