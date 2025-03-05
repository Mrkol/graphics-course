#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <chrono>

#include "etna/GraphicsPipeline.hpp"
#include "wsi/OsWindowingManager.hpp"

#include <etna/OneShotCmdMgr.hpp>


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
  std::unique_ptr<etna::OneShotCmdMgr> oneShotManager;

  etna::Sampler sampler;
  etna::Image bufImage;
  etna::GraphicsPipeline pipeline;

  etna::Image grafImage;
  etna::GraphicsPipeline graphicsPipeline;

  std::chrono::system_clock::time_point timePointStart = std::chrono::system_clock::now();
};
