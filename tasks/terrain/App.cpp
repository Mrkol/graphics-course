#include "App.hpp"

#include <algorithm>
#include <chrono>

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>


App::App()
    : resolution{1280, 720}
    , useVsync{true}
{
    {
        auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();
        std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};
        std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        etna::initialize(etna::InitParams{
            .applicationName = "Terrain",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .instanceExtensions = instanceExtensions,
            .deviceExtensions = deviceExtensions,
            .features = vk::PhysicalDeviceFeatures2{.features = {
                .tessellationShader = vk::True,
                .fillModeNonSolid = vk::True,
            }},
            .physicalDeviceIndexOverride = {},
            .numFramesInFlight = 1,
        });
    }

    auto& context = etna::get_context();
    tessellationLimit = std::min(
        tessellationLimit,
        static_cast<float>(context.getPhysicalDevice().getProperties().limits.maxTessellationGenerationLevel)
    );

    osWindow = windowing.createWindow(OsWindow::CreateInfo{
        .resolution = resolution,
    });

    {
        auto surface = osWindow->createVkSurface(context.getInstance());
        vkWindow = context.createWindow(etna::Window::CreateInfo{
            .surface = std::move(surface),
        });
    }

    commandManager = context.createPerFrameCmdMgr();

    etna::create_program("terrain heightmap", {TERRAIN_SHADERS_ROOT "heightmap.comp.spv"});
    etna::create_program("terrain", {
        TERRAIN_SHADERS_ROOT "terrain.vert.spv",
        TERRAIN_SHADERS_ROOT "terrain.tesc.spv",
        TERRAIN_SHADERS_ROOT "terrain.tese.spv",
        TERRAIN_SHADERS_ROOT "terrain.frag.spv",
    });

    sampler = etna::Sampler({
        .filter = vk::Filter::eLinear,
        .addressMode = vk::SamplerAddressMode::eClampToEdge,
        .name = "heightmap sampler",
        .maxLod = 0.0f,
    });

    GenerateHeightmap();
    RecreateSwapchain();

    camera.zNear = 0.5f;
    camera.zFar = 4000.0f;
    camera.lookAt({640, 230, 800}, {480, 75, 450}, {0, 1, 0});
}

App::~App() {
    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::GenerateHeightmap() {
    auto& context = etna::get_context();
    heightmap = context.createImage({
        .extent = vk::Extent3D{heightmapSize, heightmapSize, 1},
        .name = "four-octave Perlin heightmap",
        .format = vk::Format::eR32Sfloat,
        .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
    });

    indices = context.createBuffer({
        .size = 4 * sizeof(uint32_t),
        .bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .name = "terrain patch indices",
    });

    auto oneShot = context.createOneShotCmdMgr();
    const std::array<uint32_t, 4> corners{0, 1, 2, 3};

    auto heightmapPipeline = context.getPipelineManager().createComputePipeline("terrain heightmap", {});
    auto cmd = oneShot->start();
    ETNA_CHECK_VK_RESULT(cmd.begin(vk::CommandBufferBeginInfo{}));

    etna::set_state(cmd, indices.get(), vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferWrite);
    etna::flush_barriers(cmd);
    cmd.updateBuffer<uint32_t>(indices.get(), 0, corners);
    etna::set_state(cmd, indices.get(), vk::PipelineStageFlagBits2::eIndexInput, vk::AccessFlagBits2::eIndexRead);
    etna::flush_barriers(cmd);

    auto programInfo = etna::get_shader_program("terrain heightmap");
    const auto descriptorSet = etna::create_descriptor_set(
        programInfo.getDescriptorLayoutId(0), cmd,
        {etna::Binding{0, heightmap.genBinding({}, vk::ImageLayout::eGeneral)}}
    );
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, heightmapPipeline.getVkPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, heightmapPipeline.getVkPipelineLayout(),
        0, {descriptorSet.getVkSet()}, {}
    );
    etna::flush_barriers(cmd);
    cmd.dispatch((heightmapSize + 15) / 16, (heightmapSize + 15) / 16, 1);

    etna::set_state(
        cmd, heightmap.get(),
        vk::PipelineStageFlagBits2::eTessellationControlShader
        | vk::PipelineStageFlagBits2::eTessellationEvaluationShader
        | vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderSampledRead,
        vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd);
    ETNA_CHECK_VK_RESULT(cmd.end());
    oneShot->submitAndWait(std::move(cmd));
}

void App::CreatePipelines() {
    auto& pipelineManager = etna::get_context().getPipelineManager();
    etna::GraphicsPipeline::CreateInfo pipelineInfo{
        .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePatchList},
        .tessellationConfig = {.patchControlPoints = 4},
        .fragmentShaderOutput = {
            .colorAttachmentFormats = {vkWindow->getCurrentFormat()},
            .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    };
    pipeline = pipelineManager.createGraphicsPipeline("terrain", pipelineInfo);
    pipelineInfo.rasterizationConfig.polygonMode = vk::PolygonMode::eLine;
    wireframePipeline = pipelineManager.createGraphicsPipeline("terrain", pipelineInfo);
}

void App::RecreateSwapchain() {
    auto& context = etna::get_context();
    ETNA_CHECK_VK_RESULT(context.getDevice().waitIdle());

    auto requested = osWindow->getResolution();
    if (requested.x == 0 || requested.y == 0) return;

    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
        .resolution = {requested.x, requested.y},
        .vsync = useVsync,
        .autoGamma = false,
    });
    resolution = {w, h};
    presentSemaphores.clear();

    depth = context.createImage({
        .extent = vk::Extent3D{w, h, 1},
        .name = "terrain depth",
        .format = vk::Format::eD32Sfloat,
        .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
    });
    CreatePipelines();
}

void App::Update(float dt) {
    auto& keyboard = osWindow->keyboard;
    auto& mouse = osWindow->mouse;

    if (keyboard[KeyboardKey::kEscape] == ButtonState::Rising) osWindow->askToClose();
    if (keyboard[KeyboardKey::kF] == ButtonState::Rising) wireframe = !wireframe;
    if (keyboard[KeyboardKey::kC] == ButtonState::Rising) showLod = !showLod;
    if (mouse[MouseButton::mbRight] == ButtonState::Rising) osWindow->captureMouse = !osWindow->captureMouse;

    if (osWindow->captureMouse) {
        camera.rotate(mouse.capturedPosDelta.y * 0.12f, mouse.capturedPosDelta.x * 0.12f);
    }

    glm::vec3 direction{0};
    if (is_held_down(keyboard[KeyboardKey::kW])) direction += camera.forward();
    if (is_held_down(keyboard[KeyboardKey::kS])) direction -= camera.forward();
    if (is_held_down(keyboard[KeyboardKey::kD])) direction += camera.right();
    if (is_held_down(keyboard[KeyboardKey::kA])) direction -= camera.right();
    if (is_held_down(keyboard[KeyboardKey::kE])) direction.y += 1.0f;
    if (is_held_down(keyboard[KeyboardKey::kQ])) direction.y -= 1.0f;

    const float speed = is_held_down(keyboard[KeyboardKey::kLeftShift]) ? 240.0f : 60.0f;
    if (glm::length(direction) > 0.0f) camera.move(glm::normalize(direction) * speed * dt);

    pushConstants.viewProjection = camera.projTm(static_cast<float>(resolution.x) / resolution.y) * camera.viewTm();
    pushConstants.cameraPosition = glm::vec4(camera.position, 1.0f);
    pushConstants.terrain = {terrainSize, heightScale, tessellationLimit, 8000.0f};
}

void App::Run() {
    auto previous = std::chrono::steady_clock::now();

    while (!osWindow->isBeingClosed()) {
        windowing.poll();

        auto now = std::chrono::steady_clock::now();
        const float dt = std::min(std::chrono::duration<float>(now - previous).count(), 0.1f);
        previous = now;

        auto windowResolution = osWindow->getResolution();
        if (windowResolution.x == 0 || windowResolution.y == 0) continue;
        if (windowResolution != resolution) RecreateSwapchain();

        Update(dt);
        DrawFrame();
    }

    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::DrawTerrain(vk::CommandBuffer cmd, vk::Image backbuffer, vk::ImageView backbufferView) {
    auto programInfo = etna::get_shader_program("terrain");
    const auto descriptorSet = etna::create_descriptor_set(
        programInfo.getDescriptorLayoutId(0), cmd,
        {etna::Binding{0, heightmap.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}}
    );
    etna::set_state(cmd, indices.get(), vk::PipelineStageFlagBits2::eIndexInput, vk::AccessFlagBits2::eIndexRead);
    etna::flush_barriers(cmd);

    etna::RenderTargetState renderTargets(
        cmd,
        {{0, 0}, {resolution.x, resolution.y}},
        {{
            .image = backbuffer,
            .view = backbufferView,
            .clearColorValue = std::array<float, 4>{0.72f, 0.83f, 0.93f, 1.0f},
        }},
        {.image = depth.get(), .view = depth.getView({})}
    );

    const auto& terrainPipeline = wireframe ? wireframePipeline : pipeline;
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, terrainPipeline.getVkPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, terrainPipeline.getVkPipelineLayout(),
        0, {descriptorSet.getVkSet()}, {}
    );
    cmd.bindIndexBuffer(indices.get(), 0, vk::IndexType::eUint32);

    for (uint32_t z = 0; z < chunkCount; ++z) {
        for (uint32_t x = 0; x < chunkCount; ++x) {
            pushConstants.chunk = {x * chunkSize, z * chunkSize, chunkSize, showLod ? 1.0f : 0.0f};
            cmd.pushConstants(
                terrainPipeline.getVkPipelineLayout(),
                vk::ShaderStageFlagBits::eTessellationControl
                | vk::ShaderStageFlagBits::eTessellationEvaluation
                | vk::ShaderStageFlagBits::eFragment,
                0, sizeof(pushConstants), &pushConstants
            );
            cmd.drawIndexed(4, 1, 0, 0, 0);
        }
    }
}

void App::DrawFrame() {
    auto currentCmdBuf = commandManager->acquireNext();
    etna::begin_frame();
    auto nextSwapchainImage = vkWindow->acquireNext();

    if (nextSwapchainImage) {
        auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

        ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
        {
            DrawTerrain(currentCmdBuf, backbuffer, backbufferView);

            etna::set_state(
                currentCmdBuf, backbuffer,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput, {},
                vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor
            );
            etna::flush_barriers(currentCmdBuf);
        }
        ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

        auto renderingDone = commandManager->submit(
            std::move(currentCmdBuf), std::move(backbufferAvailableSem)
        );
        if (!Present(renderingDone, backbufferView)) nextSwapchainImage = std::nullopt;
    }

    etna::end_frame();

    if (!nextSwapchainImage) RecreateSwapchain();
}

bool App::Present(vk::Semaphore renderingDone, vk::ImageView backbufferView) {
    auto& context = etna::get_context();
    auto& presentReady = presentSemaphores[backbufferView];
    if (!presentReady) presentReady = etna::unwrap_vk_result(context.getDevice().createSemaphoreUnique({}));

    const vk::SemaphoreSubmitInfo wait{
        .semaphore = renderingDone,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    const vk::SemaphoreSubmitInfo signal{
        .semaphore = presentReady.get(),
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    const vk::SubmitInfo2 submitInfo{
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal,
    };
    ETNA_CHECK_VK_RESULT(context.getQueue().submit2({submitInfo}, {}));

    return vkWindow->present(presentReady.get(), backbufferView);
}
