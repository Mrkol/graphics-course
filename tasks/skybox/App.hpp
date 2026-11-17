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
#include "gui/ImGuiRenderer.hpp"
#include "Decals.hpp"
#include "RockModel.hpp"
#include "Skybox.hpp"


class App {
public:
    App();
    ~App();

    void Run();

private:
    void GenerateHeightmap();
    std::vector<float> ReadHeightmap();
    void CreatePipelines();
    void CreateGBuffer();
    void RecreateSwapchain();
    void Update(float dt);
    void DrawGui();
    void DrawFrame();
    void DrawScene(vk::CommandBuffer cmd, bool depthOnly);
    void DrawGeometry(vk::CommandBuffer cmd);
    void DrawLighting(vk::CommandBuffer cmd, vk::Image backbuffer, vk::ImageView backbufferView);
    bool Present(vk::Semaphore renderingDone, vk::ImageView backbufferView);

private:
    static constexpr uint32_t heightmapSize = 4096;
    static constexpr uint32_t chunkCount = 32;
    static constexpr float chunkSize = 32.0f;
    static constexpr float terrainSize = chunkCount * chunkSize;
    static constexpr float heightScale = 180.0f;
    static constexpr vk::Format depthFormat = vk::Format::eD32Sfloat;
    static constexpr vk::Format gbufferFormat = vk::Format::eR8G8B8A8Unorm;

    OsWindowingManager windowing;
    std::unique_ptr<OsWindow> osWindow;

    glm::uvec2 resolution;
    bool wireframe = false;
    bool showLod = false;
    float tessellationLimit = 64.0f;
    Camera camera;
    RockSettings rockSettings;
    std::unique_ptr<RockScatter> scatter;
    std::array<RockModel, 2> rocks;
    Decals decals;
    Skybox skybox;
    std::unique_ptr<ImGuiRenderer> guiRenderer;

    std::unique_ptr<etna::Window> vkWindow;
    std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
    std::map<vk::ImageView, vk::UniqueSemaphore> presentSemaphores;

    etna::Sampler sampler;
    etna::Sampler gbufferSampler;
    etna::Image heightmap;
    etna::Image depth;
    etna::Buffer indices;
    etna::Buffer lightingBuffer;
    etna::GraphicsPipeline depthPipeline;
    etna::GraphicsPipeline wireframeDepthPipeline;
    etna::GraphicsPipeline pipeline;
    etna::GraphicsPipeline wireframePipeline;
    etna::GraphicsPipeline lightingPipeline;

    struct GBuffer {
        etna::Image albedoRoughness;
        etna::Image normalMaterial;
    };

    GBuffer gbuffer;
    GBuffer decalBuffer;

    struct Lighting {
        glm::mat4 inverseViewProjection;
        glm::vec4 cameraPosition;
        glm::vec4 sunDirectionIntensity;
        glm::vec4 sunColor;
        glm::mat4 inverseSkyViewProjection;
    };
    static_assert(sizeof(Lighting) == 176);

    Lighting lighting{};

    struct PushConstants {
        glm::mat4 viewProjection;
        glm::vec4 cameraPosition;
        glm::vec4 chunk;
        glm::vec4 terrain;
    };

    PushConstants pushConstants{};
};
