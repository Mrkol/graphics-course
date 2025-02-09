#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/RenderTargetStates.hpp>
#include <glm/glm.hpp>

#include "Perlin.hpp"
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
  void allocateGBuffer();
  
  void loadSkybox();

  void setupPipelines(vk::Format swapchain_format);

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  
  void drawGui();

  void renderWorld(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);
private:
  void renderTerrain(
    vk::CommandBuffer cmd_buf);
  
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);

  void renderPostprocess(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

  void renderSkybox(vk::CommandBuffer cmd_buf);
  void renderLights(vk::CommandBuffer cmd_buf);
  
  void renderSphereDeferred(vk::CommandBuffer cmd_buf);
  void renderSphere(vk::CommandBuffer cmd_buf);

  void prepareFrame(const glm::mat4x4& glob_tm);

  void tonemapEvaluate(vk::CommandBuffer cmd_buf);

  void regenTerrain();
public:
  PerlinGenerator heightmap;
private:
  std::unique_ptr<SceneManager> sceneMgr;


  etna::Image backbuffer;

  std::array<etna::Image, 5> gBuffer;
  std::vector<etna::RenderTargetState::AttachmentParams> gBufferColorAttachments;
  etna::RenderTargetState::AttachmentParams gBufferDepthAttachment;

  etna::Buffer constants;

  etna::Buffer histogramBuffer;
  etna::Buffer distributionBuffer;

  etna::Buffer instanceMatricesBuf;

  struct PushConstants
  {
    glm::mat4x4 projView;
    glm::mat4x4 model;
    glm::vec4 color, emr_factors;
    glm::uint  instIdx;
  } pushConst2M;

   struct TerrainPushContants {
    glm::vec2 base, extent;
    glm::mat4x4 mat; 
    glm::vec3 camPos;
    int degree;
  } pushConstantsTerrain;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 worldView;
  glm::mat4x4 worldProj;
  
  etna::GraphicsPipeline staticMeshPipeline{};
  etna::GraphicsPipeline terrainPipeline{};
  etna::GraphicsPipeline terrainDebugPipeline{};

  etna::ComputePipeline histogramPipeline{};
  etna::ComputePipeline distributionPipeline{};

  etna::GraphicsPipeline skyboxPipeline{};
  etna::GraphicsPipeline deferredLightPipeline{};
  etna::GraphicsPipeline sphereDeferredPipeline{};
  etna::GraphicsPipeline spherePipeline{};

  etna::GraphicsPipeline postprocessPipeline{};
  etna::Sampler defaultSampler;
  glm::uvec2 resolution;

  etna::Image skybox;

  struct RenderGroup {
    RenderElement re;
    std::size_t amount;
  };
  std::vector<std::size_t> nInstances;

  bool useToneMap = false;
  bool wireframe  = false;
  bool pause      = false;
  bool normalMap  = true;
  int terrainScale = 12;

  double frameTime = 0.;
};
