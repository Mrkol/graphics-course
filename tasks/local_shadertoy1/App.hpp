#pragma once

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
  void processInput();

  struct Params {
    glm::uvec2 resolution;
    glm::vec2 mouse_pos;
    float time;
  };

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
  
  etna::ComputePipeline pipeline;
  etna::Sampler targetSampler;
  etna::Image computeImage;
  
  Params params;
  glm::vec2 mouse_pos{0.0f, 0.0f};
  std::chrono::system_clock::time_point init_time = std::chrono::system_clock::now();
};
