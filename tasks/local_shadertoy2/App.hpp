#pragma once

#include <etna/Etna.hpp>
#include <etna/Window.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <chrono>
#include <stb_image.h>

#include "wsi/OsWindowingManager.hpp"


class App
{
public:
  App();
  ~App();

  void run();

private:
  struct uniform_params
  {
    uint32_t size_x;
    uint32_t size_y;
    float time;
  };
  void drawFrame();

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
  std::unique_ptr<etna::OneShotCmdMgr> oneShotManager;

  etna::ComputePipeline computePipeline;
  etna::Image bufImage;
  etna::Sampler sampler;

  etna::GraphicsPipeline graphicsPipeline;
  etna::Image image;

  etna::Sampler graphicsSampler;

  uniform_params params;

  std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
};