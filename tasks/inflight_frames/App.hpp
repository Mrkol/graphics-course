#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"

#include <chrono>
#include <etna/Sampler.hpp>
#include <etna/BlockingTransferHelper.hpp>
#include <scene/Camera.hpp>

#include "Renderer.hpp"


class App
{

public:
  App();
  ~App();

  void processInput();
  void run();

  void rotateCam(const Mouse& ms);

private:
  void drawFrame();

private:
  Renderer renderer;

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  std::chrono::system_clock::time_point timeStart;

  Camera cam;
};