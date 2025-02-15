#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/RenderTargetStates.hpp>
#include <glm/glm.hpp>

#include "pipelines/Pipelines.hpp"
#include "targets/Backbuffer.hpp"
#include "targets/GBuffer.hpp"
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

  void renderPostprocess(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);
private:
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);

  void renderSkybox(vk::CommandBuffer cmd_buf);
  void renderLights(vk::CommandBuffer cmd_buf);
  
  void renderSphereDeferred(vk::CommandBuffer cmd_buf);
  void renderSphere(vk::CommandBuffer cmd_buf);

  void prepareFrame(const glm::mat4x4& glob_tm);

  void regenTerrain();
public:

  private:
  std::unique_ptr<SceneManager> sceneMgr;

  std::array<etna::Image, 5> gBuffer;
  std::vector<etna::RenderTargetState::AttachmentParams> gBufferColorAttachments;
  etna::RenderTargetState::AttachmentParams gBufferDepthAttachment;

  etna::Buffer constants;


  
  pipes::TerrainPipeline    terrainPipeline2{};
  pipes::StaticMeshPipeline staticMeshPipeline2{};
  
  pipes::SkyboxPipeline         skyboxPipeline2{};
  pipes::ResolveGBufferPipeline resolveGPipeline2{};

  pipes::TonemapPipeline tonemapPipeline2{};
  
  pipes::RenderContext renderContext{};

  targets::Backbuffer backbuffer2{};
  targets::GBuffer gbuffer2{};

  etna::Sampler defaultSampler;
  glm::uvec2 resolution;

  etna::Image skybox;

  struct RenderGroup {
    RenderElement re;
    std::size_t amount;
  };

  bool pause = false;

  
};
