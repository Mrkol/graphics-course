#pragma once

#include <chrono>

#include <etna/GraphicsPipeline.hpp>
#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

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
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  std::chrono::steady_clock::time_point start;
  etna::Image result;
  etna::Image bufImage;
  etna::GraphicsPipeline pipeline;
  etna::GraphicsPipeline graphicsPipeline;
  etna::Sampler sampler;

  struct PushConstants
  {
    uint32_t resolutionX, resolutionY;
    float time;
  };

  PushConstants pushConstants;
};