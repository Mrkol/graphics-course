#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <glm/glm.hpp>

#include "shaders/UniformParams.h"
#include "scene/SceneManager.hpp"
#include "render_utils/QuadRenderer.hpp"
#include "wsi/Keyboard.hpp"

#include "FramePacket.hpp"


/**
 * The meat of the sample. All things you see on the screen are contained within this class.
 * This what you want to change and expand between different samples.
 */
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


private:
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image mainViewDepth;
  etna::Image shadowMap;
  etna::Sampler defaultSampler;
  etna::Buffer constants;

  struct PushConstants
  {
    glm::mat4x4 projView;
    glm::mat4x4 model;
  } pushConst2M;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 lightMatrix;
  glm::vec3 lightPos;

  struct ShadowMapCam
  {
    float radius = 10;
    float lightTargetDist = 24;
    bool usePerspectiveM = false;
  } lightProps;

  UniformParams uniformParams{
    .baseColor = {0.9f, 0.92f, 1.0f},
  };

  etna::GraphicsPipeline basicForwardPipeline{};
  etna::GraphicsPipeline shadowPipeline{};

  std::unique_ptr<QuadRenderer> quadRenderer;
  bool drawDebugFSQuad = false;

  glm::uvec2 resolution;
};
