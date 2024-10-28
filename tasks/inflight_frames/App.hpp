#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Sampler.hpp>


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();
  void processInput();

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;
  std::chrono::system_clock::time_point timer;
  glm::vec2 mouse;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::Image image;
  etna::GlobalContext* context;

  etna::GraphicsPipeline texturePipeline{};
  etna::GraphicsPipeline graphicsPipeline{};

  etna::Sampler textureSampler;

  etna::Image texture;

  void initialize();
};
