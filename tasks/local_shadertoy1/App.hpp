#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <chrono>

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

  etna::ComputePipeline pipeline;
  etna::Image shader_image;
  etna::Sampler sampler;
  glm::vec2 mouse_pos;
  std::chrono::time_point<std::chrono::system_clock> init_time = std::chrono::system_clock::now();

  struct Params {
    glm::vec2 resolution;
    glm::vec2 mouse_pos;
    float time;
  };
  
  Params params;
};
