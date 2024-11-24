#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"

#include <etna/Sampler.hpp>


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();

private:
  const char* computeProgramName = "shader_toy";

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::ComputePipeline pipeline;
  etna::Image image;
  etna::Sampler sampler;
};
