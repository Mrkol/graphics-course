#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
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
  void setupPipelines(vk::Format swapchain_format);

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawGui();
  void renderWorld(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

  void updateCamPos(glm::vec3 pos) { 
    camPos = pos; 
  }
private:
  void renderTerrain(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);
  
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);

  void prepareFrame(const glm::mat4x4& glob_tm);
public:
  PerlinGenerator heightmap;
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

  struct TerrainPushContants {
    glm::vec2 base, extent;
    glm::mat4x4 mat; 
    glm::vec3 camPos;
    int degree;
  } pushConstantsTerrain;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 lightMatrix;
  etna::GraphicsPipeline staticMeshPipeline{};
  etna::GraphicsPipeline terrainPipeline{};
  etna::GraphicsPipeline terrainDebugPipeline{};

  bool wireframe = false;

  glm::uvec2 resolution;
  glm::vec3 camPos;

  struct RenderGroup {
    RenderElement re;
    std::size_t amount;
  };
  std::vector<std::size_t> nInstances;
};
