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

  bool  getWireframe() const            { return settings_.wireframe; }
  void  setWireframe(bool v)            { settings_.wireframe = v; }

  bool  getBaseColorOnly() const        { return settings_.baseColorOnly; }
  void  setBaseColorOnly(bool v)        { settings_.baseColorOnly = v; }

  bool  getShowNormals() const          { return settings_.showNormals; }
  void  setShowNormals(bool v)          { settings_.showNormals = v; }

  float getGamma() const                { return settings_.gamma; }
  void  setGamma(float v)               { settings_.gamma = v; }

  float getExposure() const             { return settings_.exposure; }
  void  setExposure(float v)            { settings_.exposure = v; }

  glm::vec3 getLightPos() const         { return lightPos; }
  void  setLightPos(const glm::vec3& v) { lightPos = v; }

  glm::vec3 getBaseColor() const        { return uniformParams.baseColor; }
  void      setBaseColor(const glm::vec3& v) { uniformParams.baseColor = v; }

  glm::vec3 getLightColor() const       { return lightColor_; }
  void  setLightColor(const glm::vec3& v) { lightColor_ = v; }

  float getLightIntensity() const   { return lightIntensity_; }
  void  setLightIntensity(float v)  { lightIntensity_ = v; }

  int   getShadowMapResIndex() const    { return settings_.shadowResIndex; }
  void  setShadowMapResIndex(int i)     { settings_.shadowResIndex = i; }

  float getShadowBias() const           { return settings_.shadowBias; }
  void  setShadowBias(float v)          { settings_.shadowBias = v; }

  bool  getShadowPCF() const            { return settings_.shadowPCF; }
  void  setShadowPCF(bool v)            { settings_.shadowPCF = v; }

  void  setTimeScale(float s)           { settings_.timeScale = s; }
  float getTimeScale() const            { return settings_.timeScale; }

  void  saveScreenshot();

  enum class AAMode { None = 0, FXAA = 1 };
  AAMode getAAMode() const { return aaMode_; }
  void   setAAMode(AAMode m) { aaMode_ = m; }

  bool getPurpleTint() const { return purpleTint_; }
  void setPurpleTint(bool v) { purpleTint_ = v; }

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
  glm::vec3   lightPos { 10.0f, 15.0f, 10.0f };

  glm::vec3 lightColor_ { 1.0f, 1.0f, 1.0f };
  float lightIntensity_  = 1.0f;

  struct ShadowMapCam
  {
    float radius = 10;
    float lightTargetDist = 24;
    bool  usePerspectiveM = false;
  } lightProps;

  UniformParams uniformParams{
    .lightMatrix = {},
    .lightPos    = {},
    .time        = {},
    .baseColor   = {0.9f, 0.92f, 1.0f},
  };


  AAMode aaMode_ = AAMode::FXAA;

  etna::Image mainColor;
  etna::Sampler postSampler;

  etna::GraphicsPipeline basicForwardPipeline;
  etna::GraphicsPipeline basicForwardOffscreenPipeline;
  etna::GraphicsPipeline fxaaPipeline;
  etna::GraphicsPipeline shadowPipeline;

  std::unique_ptr<QuadRenderer> quadRenderer;
  bool drawDebugFSQuad = false;
  glm::uvec2 resolution;

  struct RenderSettings {
    bool  wireframe      = false;
    bool  baseColorOnly  = false;
    bool  showNormals    = false;

    float gamma          = 2.2f;
    float exposure       = 1.0f;

    int   shadowResIndex = 1;
    float shadowBias     = 0.0015f;
    bool  shadowPCF      = true;

    float timeScale      = 1.0f;
  } settings_;

  bool purpleTint_ = false;
};