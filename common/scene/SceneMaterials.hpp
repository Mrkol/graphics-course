#pragma once

#include <glm/glm.hpp>

#include <tiny_gltf.h>

#include <etna/BlockingTransferHelper.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>


struct Texture
{
  std::size_t image;
  std::size_t sampler;
};

struct Material
{
  std::optional<std::size_t> baseColorTexture;
  glm::vec4 baseColor{glm::vec4(1.0, 1.0, 1.0, 1.0)};

  std::optional<std::size_t> metallicRoughnessTexture;
  float metallicFactor{0.0};
  float roughnessFactor{0.0};

  std::optional<std::size_t> normalTexture;
  std::optional<std::size_t> occlusionTexture;

  std::optional<std::size_t> emissiveTexture;
  glm::vec4 emissiveFactor{0.0};
};

class SceneMaterials
{
public:
  void load(const tinygltf::Model& model, etna::OneShotCmdMgr& one_shot_cmd_mgr);

  const etna::Image& getImage(std::size_t image_index) const;
  const etna::Sampler& getSampler(std::size_t sampler_index) const;
  const Texture& getTexture(std::size_t texture_index) const;
  const Material& getMaterial(std::size_t material_index) const;

private:
  std::vector<etna::Image> images;
  std::vector<etna::Sampler> samplers;
  std::vector<Texture> textures;

  std::vector<Material> materials;
};
