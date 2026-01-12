#pragma once

#include <chrono>
#include <string>
#include <utility>

#include <glm/glm.hpp>

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();
  void recordShadertoy(vk::CommandBuffer cmd_buf, vk::Image backbuffer);
  void createComputePipeline();
  void recreateStorageImage();
  void ensureStorageImage();
  glm::uvec2 calcDispatchGroupCount() const;
  std::pair<float, float> pullFrameTiming();

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::ComputePipeline shadertoyPipeline;
  etna::Image storageImage;
  glm::uvec2 storageImageExtent{0, 0};
  vk::Format storageImageFormat = vk::Format::eR8G8B8A8Unorm;
  glm::uvec2 workgroupSize{32u, 32u};
  bool storageImageInitialized = false;

  double startTimeSeconds = 0.0;
  double lastFrameTimestamp = 0.0;
};
