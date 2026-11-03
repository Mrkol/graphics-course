#pragma once

#include <map>

#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/Sampler.hpp>
#include <etna/Window.hpp>

#include "scene/Camera.hpp"
#include "wsi/OsWindowingManager.hpp"


class App {
    friend struct TerrainTest;

public:
    App();
    ~App();

    void Run();

private:
    void GenerateHeightmap();
    void CreatePipelines();
    void RecreateSwapchain();
    void Update(float dt);
    void DrawFrame();
    void DrawTerrain(vk::CommandBuffer cmd, vk::Image backbuffer, vk::ImageView backbufferView);
    bool Present(vk::Semaphore renderingDone, vk::ImageView backbufferView);

private:
    static constexpr uint32_t heightmapSize = 4096;
    static constexpr uint32_t chunkCount = 32;
    static constexpr float chunkSize = 32.0f;
    static constexpr float terrainSize = chunkCount * chunkSize;
    static constexpr float heightScale = 180.0f;

    OsWindowingManager windowing;
    std::unique_ptr<OsWindow> osWindow;

    glm::uvec2 resolution;
    bool useVsync;
    bool wireframe = false;
    bool showLod = false;
    float tessellationLimit = 64.0f;
    Camera camera;

    std::unique_ptr<etna::Window> vkWindow;
    std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
    std::map<vk::ImageView, vk::UniqueSemaphore> presentSemaphores;

    etna::Sampler sampler;
    etna::Image heightmap;
    etna::Image depth;
    etna::Buffer indices;
    etna::GraphicsPipeline pipeline;
    etna::GraphicsPipeline wireframePipeline;

    struct PushConstants {
        glm::mat4 viewProjection;
        glm::vec4 cameraPosition;
        glm::vec4 chunk;
        glm::vec4 terrain;
    };

    PushConstants pushConstants{};
};
