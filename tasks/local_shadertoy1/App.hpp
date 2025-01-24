#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"
#include <etna/Sampler.hpp>
#include <chrono>

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

  etna::Sampler sampler;
  etna::Image buffImage;
  etna::ComputePipeline pipeline;

  struct {
    uint32_t size_x;
    uint32_t size_y;
    float time;
    float mouse_x;
    float mouse_y;
  } pushedParams;

  std::chrono::system_clock::time_point start = std::chrono::system_clock::now();


  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
};
