#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/GraphicsPipeline.hpp>
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
  auto getParams();
  etna::Image loadTexture(const std::string &path, const std::string &name);

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::GraphicsPipeline toyPipeline;
  etna::GraphicsPipeline genPipeline;
  etna::Image gtxt;
  etna::Image skytxt;
  etna::Image gentxt;
  etna::Sampler sampler;
};
