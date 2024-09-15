#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <glm/glm.hpp>

#include "render_utils/LineRenderer.hpp"
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
  struct MaterialBindings
  {
    etna::ImageBinding baseColorBinding;
    etna::ImageBinding metallicRoughnessBinding;
    etna::ImageBinding normalBinding;
    etna::ImageBinding occlusionBinding;
    etna::ImageBinding emissiveBinding;
  };

  struct MaterialConstants
  {
    glm::vec4 baseColorMetallicFactor;
    glm::vec4 emissiveRoughnessFactors;
  };

  etna::DescriptorSet createMaterialBindings(
    vk::CommandBuffer command_buffer, std::optional<std::size_t> material_index) const;

  MaterialConstants getMaterialConstants(std::optional<std::size_t> material_index) const;

  void doGBufferPass(vk::CommandBuffer cmd_buf);
  void doLightingPasses(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

private:
  // Scene manager

  std::unique_ptr<SceneManager> sceneMgr;

  // "Default" resources

  etna::Sampler defaultSampler;

  etna::Image defaultBaseColorImage;
  etna::Image defaultMetallicRoughnessImage;
  etna::Image defaultNormalImage;
  etna::Image defaultOcclusionImage;
  etna::Image defaultEmissiveImage;

  // Co;or attachments for light passes

  struct GBuffer
  {
    etna::Image baseColor;
    etna::Image normal;
    etna::Image emissive;
    etna::Image occlusionMetallicRoughness;
    etna::Image depth;
  };

  GBuffer gBuffer;

  // View-projection matrix

  glm::mat4 viewProjection;

  // Pipelines

  etna::GraphicsPipeline gBufferPassPipeline;

  std::array<etna::GraphicsPipeline, std::tuple_size_v<KnownLightTypes>> lightPassPipelines;
  etna::GraphicsPipeline emissiveLightPassPipeline;

  void initGBufferPipeline();
  void initLightPassPipelines(vk::Format swapchain_format);

  // Debug stuff

  enum struct DebugQuadMode
  {
    kDisabled,
    kBaseColor,
    kNormal,
    kEmissive,
    kOcclusionMetallicRoughness,
    kDepth,
  };

  std::unique_ptr<QuadRenderer> quadRenderer;
  DebugQuadMode debugQuadMode{DebugQuadMode::kDisabled};

  void initQuadRenderer(vk::Format swapchain_format);

  std::unique_ptr<LineRenderer> gridRenderer;
  bool drawDebugGrid = false;

  void initGridRenderer(vk::Format swapchain_format);

  // Framebuffer size

  glm::uvec2 resolution;
};
