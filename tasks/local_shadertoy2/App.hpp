#pragma once

#include <etna/Sampler.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GraphicsPipeline.hpp>
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

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  glm::uvec2 skinTextureResolution;
  bool useVsync;

  etna::GraphicsPipeline skinTexturePipeline;
  etna::GraphicsPipeline fragVertPipeline;
  etna::Image skinTextureImage;
  etna::Image fileTextureImage;
  etna::Sampler defaultSampler;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
  std::unique_ptr<etna::OneShotCmdMgr> oneShotManager;

  bool initializedFileTexture = false;
};