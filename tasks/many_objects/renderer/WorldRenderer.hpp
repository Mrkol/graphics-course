#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <glm/glm.hpp>

#include "scene/SceneManager.hpp"
#include "wsi/Keyboard.hpp"

#include "FramePacket.hpp"


class WorldRenderer
{
public:
  WorldRenderer();

  void loadScene(std::filesystem::path path);

  void loadShaders();
  void allocateResources(glm::uvec2 swapchain_resolution);
  void setupPipelines(vk::Format swapchain_format);

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawGui();
  void renderWorld(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

private:
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);

  void prepareFrame(const glm::mat4x4& glob_tm);
private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image mainViewDepth;
  etna::Buffer constants;

  etna::Buffer instanceMatricesBuf;

  struct PushConstants
  {
    glm::mat4x4 projView;
    glm::mat4x4 model;
    glm::uint  instIdx;
  } pushConst2M;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 lightMatrix;

  etna::GraphicsPipeline staticMeshPipeline{};

  glm::uvec2 resolution;

  struct RenderGroup {
    RenderElement re;
    std::size_t amount;
  };
  std::vector<std::size_t> nInstances;
};
