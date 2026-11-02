#pragma once
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>
#include <glm/glm.hpp>
#include "wsi/Keyboard.hpp"
#include "FramePacket.hpp"
#include "RenderSettings.hpp"
#include "RockField.hpp"

class WorldRenderer
{
public:
  void allocateResources(glm::uvec2 swapchain_resolution);
  void resizeDepth(glm::uvec2 swapchain_resolution);
  void loadShaders();
  void setupPipelines(vk::Format swapchain_format);
  void debugInput(const Keyboard&);
  void update(const FramePacket& packet);
  void drawGui();
  void renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_view, uint32_t frameSlot);

private:
  NoiseSettings noise;
  DrawSettings draw;
  RockField rocks;

  void generateMaps(vk::CommandBuffer cmd_buf);
  static constexpr uint32_t kGridCells = 256;
  static constexpr uint32_t kIndexCount = kGridCells * kGridCells * 6;

  etna::Image depth;
  etna::Image heightMap;
  etna::Image albedoAtlas;
  etna::Image roughnessAtlas;
  etna::Image detailHeightAtlas;
  etna::Sampler sampler;
  etna::Buffer indices;
  etna::GraphicsPipeline drawPipeline{};
  etna::ComputePipeline generatePipeline{};
  glm::uvec2 resolution{};
  bool pendingGeneration = true;
};
