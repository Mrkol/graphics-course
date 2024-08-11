#pragma once

#include <etna/GraphicsPipeline.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/Sampler.hpp>
#include <glm/fwd.hpp>

#include "wsi/Keyboard.hpp"
#include "scene/SceneManager.hpp"
#include "scene/Camera.h"
#include "render_utils/QuadRenderer.hpp"
#include "../../resources/shaders/shader_common.h"


class ImGuiRenderer;

using ResolutionProvider = fu2::unique_function<glm::uvec2() const>;

class Renderer
{
public:
  explicit Renderer(glm::uvec2 resolution);
  ~Renderer();

  void initVulkan(std::span<const char*> instance_extensions);
  void initPresentation(vk::UniqueSurfaceKHR surface, ResolutionProvider res_provider);

  void debugInput(const Keyboard& kb);

  void updateView(const Camera &main, const Camera &shadow);

  void drawFrame(float dt);

  void loadScene(std::filesystem::path path);

  void recreateSwapchain(glm::uvec2 res);

private:
  etna::GlobalContext* context;
  etna::Image mainViewDepth;
  etna::Image shadowMap;
  etna::Sampler defaultSampler;
  etna::Buffer constants;

  ResolutionProvider resolutionProvider;
  std::unique_ptr<etna::Window> window;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  struct PushConstants
  {
    glm::mat4x4 projView;
    glm::mat4x4 model;
  } pushConst2M;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 lightMatrix;
  glm::vec3 lightPos;

  UniformParams uniformParams {};

  etna::GraphicsPipeline basicForwardPipeline {};
  etna::GraphicsPipeline shadowPipeline {};

  glm::uvec2 resolution;

  std::unique_ptr<SceneManager> sceneMgr;
  std::unique_ptr<ImGuiRenderer> guiRenderer;

  std::unique_ptr<QuadRenderer> quadRenderer;

  bool drawDebugFSQuad = false;

  struct ShadowMapCam
  {
    float  radius = 10;
    float  lightTargetDist = 24;
    bool   usePerspectiveM = false;
  } lightProps;

  void drawFrame(bool draw_gui);

  void renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

  void renderScene(vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);

  void loadShaders();

  void setupPipelines();

  void updateUniformBuffer(float time);

  void allocateResources();
  void preparePipelines();

  void DrawGui();
};
