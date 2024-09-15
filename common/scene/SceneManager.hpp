#pragma once

#include <filesystem>

#include <glm/glm.hpp>
#include <tiny_gltf.h>
#include <etna/Buffer.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <etna/VertexInput.hpp>
#include <etna/Sampler.hpp>

#include "SceneLights.hpp"
#include "SceneMaterials.hpp"
#include "SceneMeshes.hpp"


class SceneManager
{
public:
  SceneManager();

  void selectScene(std::filesystem::path path);

  std::span<const glm::mat4x4> getInstanceMatrices() { return instanceMatrices; }

  const SceneMeshes& getMeshes() const { return meshes; }
  const SceneLights& getLights() const { return lights; }
  const SceneMaterials& getMaterials() const { return materials; }

private:
  std::optional<tinygltf::Model> loadModel(std::filesystem::path path);

  std::vector<glm::mat4> processInstanceMatrices(const tinygltf::Model& model) const;

private:
  tinygltf::TinyGLTF loader;
  std::unique_ptr<etna::OneShotCmdMgr> oneShotCommands;
  etna::BlockingTransferHelper transferHelper;

  std::vector<glm::mat4> instanceMatrices;

  SceneMeshes meshes;
  SceneLights lights;
  SceneMaterials materials;
};
