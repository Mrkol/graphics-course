#pragma once

#include "etna/Sampler.hpp"


#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>

#include "wsi/OsWindowingManager.hpp"


class App
{
public:
  App();
  ~App();

  void run();
  void buildComputeCommands(vk::CommandBuffer currentCmdBuf);
  vk::ImageCopy buildFullImageCopy();

private:
  void drawFrame();

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::ComputePipeline computePipeline;
  etna::Image image;
  etna::Sampler defaultSampler;
  std::chrono::steady_clock::time_point startTime;
};
