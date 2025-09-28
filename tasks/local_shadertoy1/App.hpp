#pragma once

#include <chrono>

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
    
    void update();
private:
    OsWindowingManager windowing;
    std::unique_ptr<OsWindow> osWindow;

    glm::uvec2 resolution;
    bool useVsync;

    std::unique_ptr<etna::Window> vkWindow;
    std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

    etna::Sampler sampler;
    etna::Image result;
    etna::ComputePipeline pipeline;

    struct PushConstants {
        uint32_t resolutionX, resolutionY;
        float mouseX, mouseY;
        float time;
    };

    PushConstants pushConstants;
    std::chrono::steady_clock::time_point start;
};
