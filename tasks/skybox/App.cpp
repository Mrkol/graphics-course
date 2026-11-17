#include "App.hpp"
#include "StartupCommands.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <initializer_list>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>


namespace {

struct ImageState {
    vk::ImageLayout layout;
    vk::PipelineStageFlags2 stages;
    vk::AccessFlags2 access;
};

constexpr auto discarded = ImageState{vk::ImageLayout::eUndefined, {}, {}};
constexpr auto depthWrite = ImageState{
    vk::ImageLayout::eDepthStencilAttachmentOptimal,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
};
constexpr auto depthLoad = ImageState{
    depthWrite.layout, depthWrite.stages, depthWrite.access | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
};
constexpr auto colorWrite = ImageState{
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
};
constexpr auto sampled = ImageState{
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderSampledRead,
};

vk::ImageMemoryBarrier2 ImageBarrier(const etna::Image& image, ImageState before, ImageState after) {
    return {
        .srcStageMask = before.stages,
        .srcAccessMask = before.access,
        .dstStageMask = after.stages,
        .dstAccessMask = after.access,
        .oldLayout = before.layout,
        .newLayout = after.layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image.get(),
        .subresourceRange = {.aspectMask = image.getAspectMaskByFormat(), .levelCount = 1, .layerCount = 1},
    };
}

void TransitionImages(vk::CommandBuffer cmd, std::initializer_list<vk::ImageMemoryBarrier2> barriers) {
    cmd.pipelineBarrier2({
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.begin(),
    });
}

}

App::App()
    : resolution(1280, 720)
{
    {
        auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();
        auto instanceExtensions = std::vector<const char*>(glfwInstExts.begin(), glfwInstExts.end());
        auto deviceExtensions = std::array{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        etna::initialize(etna::InitParams{
            .applicationName = "Procedural sky",
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

    etna::create_program("terrain heightmap", {SKYBOX_SHADERS_ROOT "heightmap.comp.spv"});
    etna::create_program("terrain depth", {
        SKYBOX_SHADERS_ROOT "terrain.vert.spv",
        SKYBOX_SHADERS_ROOT "terrain.tesc.spv",
        SKYBOX_SHADERS_ROOT "terrain.tese.spv",
    });
    etna::create_program("terrain", {
        SKYBOX_SHADERS_ROOT "terrain.vert.spv",
        SKYBOX_SHADERS_ROOT "terrain.tesc.spv",
        SKYBOX_SHADERS_ROOT "terrain.tese.spv",
        SKYBOX_SHADERS_ROOT "terrain.frag.spv",
    });
    etna::create_program("rocks depth", {SKYBOX_SHADERS_ROOT "rocks.vert.spv"});
    etna::create_program("instanced rocks", {
        SKYBOX_SHADERS_ROOT "rocks.vert.spv",
        SKYBOX_SHADERS_ROOT "rocks.frag.spv",
    });
    etna::create_program("deferred lighting", {
        SKYBOX_SHADERS_ROOT "fullscreen.vert.spv",
        SKYBOX_SHADERS_ROOT "lighting.frag.spv",
    });

    sampler = etna::Sampler({
        .filter = vk::Filter::eLinear,
        .addressMode = vk::SamplerAddressMode::eClampToEdge,
        .name = "heightmap sampler",
        .maxLod = 0.0f,
    });
    gbufferSampler = etna::Sampler({
        .filter = vk::Filter::eNearest,
        .addressMode = vk::SamplerAddressMode::eClampToEdge,
        .name = "G-buffer sampler",
        .maxLod = 0.0f,
    });
    lightingBuffer = context.createBuffer({
        .size = sizeof(Lighting),
        .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .name = "deferred lighting uniforms",
    });

    spdlog::info("Generating terrain heightmap");
    GenerateHeightmap();
    spdlog::info("Reading terrain heightmap for rock placement");
    scatter = std::make_unique<RockScatter>(ReadHeightmap(), heightmapSize, terrainSize, heightScale);
    rocks[0].Load(GRAPHICS_COURSE_RESOURCES_ROOT "/rocks/boulder_01/boulder_01_1k.gltf");
    rocks[1].Load(GRAPHICS_COURSE_RESOURCES_ROOT "/rocks/rock_09/rock_09_1k.gltf");
    decals.Load(*scatter);
    skybox.Load();
    RecreateSwapchain();
    guiRenderer = std::make_unique<ImGuiRenderer>(vkWindow->getCurrentFormat());
    ImGuiRenderer::enableImGuiForWindow(osWindow->native());

    camera.zNear = 0.5f;
    camera.zFar = 4000.0f;
    auto cameraHeight = scatter->SampleHeight({640, 800}) + 8.0f;
    camera.lookAt({640, cameraHeight, 800}, {600, cameraHeight, 720}, {0, 1, 0});
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
        .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
    });

    indices = context.createBuffer({
        .size = 4 * sizeof(uint32_t),
        .bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .name = "terrain patch indices",
    });

    auto startup = StartupCommands(context);
    auto corners = std::array<uint32_t, 4>{0, 1, 2, 3};

    auto heightmapPipeline = context.getPipelineManager().createComputePipeline("terrain heightmap", {});
    auto cmd = startup.Begin();

    etna::set_state(cmd, indices.get(), vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferWrite);
    etna::flush_barriers(cmd);
    cmd.updateBuffer<uint32_t>(indices.get(), 0, corners);
    etna::set_state(cmd, indices.get(), vk::PipelineStageFlagBits2::eIndexInput, vk::AccessFlagBits2::eIndexRead);
    etna::flush_barriers(cmd);

    auto programInfo = etna::get_shader_program("terrain heightmap");
    auto descriptorSet = etna::create_descriptor_set(
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
    startup.SubmitAndWait();
}

void App::CreatePipelines() {
    auto& pipelineManager = etna::get_context().getPipelineManager();
    auto colorFormats = std::array{gbufferFormat, gbufferFormat};
    auto pipelineInfo = etna::GraphicsPipeline::CreateInfo{
        .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePatchList},
        .tessellationConfig = {.patchControlPoints = 4},
        .fragmentShaderOutput = {
            .depthAttachmentFormat = depthFormat,
        },
    };
    auto blend = pipelineInfo.blendingConfig.attachments.front();
    pipelineInfo.blendingConfig.attachments.clear();
    depthPipeline = pipelineManager.createGraphicsPipeline("terrain depth", pipelineInfo);
    pipelineInfo.rasterizationConfig.polygonMode = vk::PolygonMode::eLine;
    wireframeDepthPipeline = pipelineManager.createGraphicsPipeline("terrain depth", pipelineInfo);

    pipelineInfo.blendingConfig.attachments.assign(colorFormats.size(), blend);
    pipelineInfo.depthConfig.depthCompareOp = vk::CompareOp::eEqual;
    pipelineInfo.depthConfig.depthWriteEnable = vk::False;
    pipelineInfo.fragmentShaderOutput.colorAttachmentFormats.assign(colorFormats.begin(), colorFormats.end());
    pipelineInfo.rasterizationConfig.polygonMode = vk::PolygonMode::eFill;
    pipeline = pipelineManager.createGraphicsPipeline("terrain", pipelineInfo);
    pipelineInfo.rasterizationConfig.polygonMode = vk::PolygonMode::eLine;
    wireframePipeline = pipelineManager.createGraphicsPipeline("terrain", pipelineInfo);
    for (auto& rock : rocks) rock.CreatePipelines(colorFormats, depthFormat);
    decals.CreatePipeline(colorFormats);
    lightingPipeline = pipelineManager.createGraphicsPipeline("deferred lighting", {
        .depthConfig = {.depthTestEnable = vk::False, .depthWriteEnable = vk::False},
        .fragmentShaderOutput = {.colorAttachmentFormats = {vkWindow->getCurrentFormat()}},
    });
}

std::vector<float> App::ReadHeightmap() {
    auto& context = etna::get_context();
    auto device = context.getDevice();
    auto byteSize = static_cast<vk::DeviceSize>(heightmapSize) * heightmapSize * sizeof(float);

    auto memory = vk::UniqueDeviceMemory();
    auto buffer = etna::unwrap_vk_result(device.createBufferUnique({
        .size = byteSize,
        .usage = vk::BufferUsageFlagBits::eTransferDst,
    }));
    auto requirements = device.getBufferMemoryRequirements(buffer.get());
    auto properties = context.getPhysicalDevice().getMemoryProperties();
    auto memoryType = properties.memoryTypeCount;
    for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
        if (
            (requirements.memoryTypeBits & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)
        ) {
            memoryType = i;
            break;
        }
    }
    ETNA_VERIFYF(memoryType < properties.memoryTypeCount, "No host-visible heightmap readback memory");
    memory = etna::unwrap_vk_result(device.allocateMemoryUnique({
        .allocationSize = requirements.size,
        .memoryTypeIndex = memoryType,
    }));
    ETNA_CHECK_VK_RESULT(device.bindBufferMemory(buffer.get(), memory.get(), 0));

    auto startup = StartupCommands(context);
    auto cmd = startup.Begin();
    etna::set_state(
        cmd,
        heightmap.get(),
        vk::PipelineStageFlagBits2::eCopy,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageAspectFlagBits::eColor
    );

    etna::flush_barriers(cmd);
    cmd.copyImageToBuffer(
        heightmap.get(),
        vk::ImageLayout::eTransferSrcOptimal,
        buffer.get(),
        {
            vk::BufferImageCopy{
                .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
                .imageExtent = heightmap.getExtent(),
            },
        }
    );

    auto barrier = vk::BufferMemoryBarrier2{
        .srcStageMask = vk::PipelineStageFlagBits2::eCopy,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eHost,
        .dstAccessMask = vk::AccessFlagBits2::eHostRead,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .buffer = buffer.get(),
        .size = vk::WholeSize,
    };

    cmd.pipelineBarrier2({.bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &barrier});
    etna::set_state(
        cmd, heightmap.get(),
        vk::PipelineStageFlagBits2::eTessellationControlShader
        | vk::PipelineStageFlagBits2::eTessellationEvaluationShader
        | vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderSampledRead, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd);
    startup.SubmitAndWait();

    auto mapped = etna::unwrap_vk_result(device.mapMemory(memory.get(), 0, vk::WholeSize));
    ETNA_CHECK_VK_RESULT(device.invalidateMappedMemoryRanges({vk::MappedMemoryRange{.memory = memory.get(), .size = vk::WholeSize}}));
    auto values = std::vector<float>(static_cast<std::size_t>(heightmapSize) * heightmapSize);
    std::memcpy(values.data(), mapped, static_cast<std::size_t>(byteSize));
    device.unmapMemory(memory.get());
    return values;
}

void App::DrawGui() {
    ImGui::Begin("Rock instancing");
    ImGui::SliderFloat("Density", &rockSettings.density, 0.0f, 1.0f);
    ImGui::SliderFloat("Draw distance", &rockSettings.drawDistance, 20.0f, 600.0f, "%.0f");
    ImGui::DragFloatRange2("Size range", &rockSettings.minSize, &rockSettings.maxSize, 0.05f, 0.2f, 8.0f);
    rockSettings.maxSize = std::max(rockSettings.minSize, rockSettings.maxSize);
    ImGui::Text("Boulder 01: %u", rocks[0].GetInstanceCount());
    ImGui::Text("Rock 09: %u", rocks[1].GetInstanceCount());
    auto drawCalls = (rocks[0].GetInstanceCount() > 0 ? 1u : 0u) + (rocks[1].GetInstanceCount() > 0 ? 1u : 0u);
    ImGui::Text("Rock draw calls per pass: %u", drawCalls);
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
    ImGui::TextUnformatted("WASD / Q / E: move; RMB: mouse look; Shift: speed");
    decals.DrawGui();
    ImGui::End();
    skybox.DrawGui();
}

void App::CreateGBuffer() {
    auto& context = etna::get_context();
    auto createImage = [&](const char* name) {
        return context.createImage({
            .extent = vk::Extent3D{resolution.x, resolution.y, 1},
            .name = name,
            .format = gbufferFormat,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        });
    };
    gbuffer = {createImage("G-buffer albedo and roughness"), createImage("G-buffer normal and material")};
    decalBuffer = {createImage("Decal albedo and roughness"), createImage("Decal normal and material")};
    CreatePipelines();
}

void App::RecreateSwapchain() {
    auto& context = etna::get_context();
    ETNA_CHECK_VK_RESULT(context.getDevice().waitIdle());

    auto requested = osWindow->getResolution();
    if (requested.x == 0 || requested.y == 0) return;

    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
        .resolution = {requested.x, requested.y},
        .vsync = true,
        .autoGamma = false,
    });
    resolution = {w, h};
    presentSemaphores.clear();

    depth = context.createImage({
        .extent = vk::Extent3D{w, h, 1},
        .name = "scene depth",
        .format = depthFormat,
        .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
    });
    CreateGBuffer();
}

void App::Update(float dt) {
    auto& keyboard = osWindow->keyboard;
    auto& mouse = osWindow->mouse;

    if (ImGui::GetIO().WantCaptureKeyboard) {
        pushConstants.viewProjection = camera.projTm(static_cast<float>(resolution.x) / resolution.y) * camera.viewTm();
        pushConstants.cameraPosition = glm::vec4(camera.position, 1.0f);
        pushConstants.terrain = {terrainSize, heightScale, tessellationLimit, 8000.0f};
        return;
    }

    if (keyboard[KeyboardKey::kEscape] == ButtonState::Rising) osWindow->askToClose();
    if (keyboard[KeyboardKey::kF] == ButtonState::Rising) wireframe = !wireframe;
    if (keyboard[KeyboardKey::kC] == ButtonState::Rising) showLod = !showLod;
    if (!ImGui::GetIO().WantCaptureMouse && mouse[MouseButton::mbRight] == ButtonState::Rising) {
        osWindow->captureMouse = !osWindow->captureMouse;
    }

    if (osWindow->captureMouse) {
        camera.rotate(mouse.capturedPosDelta.y * 0.12f, mouse.capturedPosDelta.x * 0.12f);
    }

    auto direction = glm::vec3(0.0f);
    if (is_held_down(keyboard[KeyboardKey::kW])) direction += camera.forward();
    if (is_held_down(keyboard[KeyboardKey::kS])) direction -= camera.forward();
    if (is_held_down(keyboard[KeyboardKey::kD])) direction += camera.right();
    if (is_held_down(keyboard[KeyboardKey::kA])) direction -= camera.right();
    if (is_held_down(keyboard[KeyboardKey::kE])) direction.y += 1.0f;
    if (is_held_down(keyboard[KeyboardKey::kQ])) direction.y -= 1.0f;

    auto speed = is_held_down(keyboard[KeyboardKey::kLeftShift]) ? 240.0f : 60.0f;
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
        auto dt = std::min(std::chrono::duration<float>(now - previous).count(), 0.1f);
        previous = now;

        auto windowResolution = osWindow->getResolution();
        if (windowResolution.x == 0 || windowResolution.y == 0) continue;
        if (windowResolution != resolution) RecreateSwapchain();

        guiRenderer->nextFrame();
        ImGui::NewFrame();
        DrawGui();
        Update(dt);
        ImGui::Render();
        skybox.BakeIfDirty();
        DrawFrame();
    }

    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::DrawScene(vk::CommandBuffer cmd, bool depthOnly) {
    auto programInfo = etna::get_shader_program(depthOnly ? "terrain depth" : "terrain");
    auto descriptorSet = etna::create_descriptor_set(
        programInfo.getDescriptorLayoutId(0), cmd,
        {etna::Binding{0, heightmap.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}},
        BarrierBehavior::eSuppressBarriers
    );

    auto& terrainPipeline = depthOnly
        ? (wireframe ? wireframeDepthPipeline : depthPipeline)
        : (wireframe ? wireframePipeline : pipeline);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, terrainPipeline.getVkPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        terrainPipeline.getVkPipelineLayout(),
        0,
        { descriptorSet.getVkSet() },
        {}
    );
    cmd.bindIndexBuffer(indices.get(), 0, vk::IndexType::eUint32);
    auto stages = vk::ShaderStageFlagBits::eTessellationControl | vk::ShaderStageFlagBits::eTessellationEvaluation;
    if (!depthOnly) stages |= vk::ShaderStageFlagBits::eFragment;

    for (uint32_t z = 0; z < chunkCount; z++) {
        for (uint32_t x = 0; x < chunkCount; x++) {
            pushConstants.chunk = {x * chunkSize, z * chunkSize, chunkSize, showLod ? 1.0f : 0.0f};
            cmd.pushConstants(
                terrainPipeline.getVkPipelineLayout(),
                stages,
                0, sizeof(pushConstants), &pushConstants
            );
            cmd.drawIndexed(4, 1, 0, 0, 0);
        }
    }
    for (auto& rock : rocks) rock.Draw(cmd, pushConstants.viewProjection, depthOnly);
}

void App::DrawGeometry(vk::CommandBuffer cmd) {
    auto rect = vk::Rect2D{{0, 0}, {resolution.x, resolution.y}};

    TransitionImages(cmd, {ImageBarrier(depth, discarded, depthWrite)});
    {
        auto renderTargets = etna::RenderTargetState(
            cmd, rect, {}, {.image = depth.get(), .view = depth.getView({})}, BarrierBehavior::eSuppressBarriers
        );
        DrawScene(cmd, true);
    }
    TransitionImages(cmd, {
        ImageBarrier(depth, depthWrite, depthLoad),
        ImageBarrier(gbuffer.albedoRoughness, discarded, colorWrite),
        ImageBarrier(gbuffer.normalMaterial, discarded, colorWrite),
    });
    auto renderTargets = etna::RenderTargetState(
        cmd, rect,
        {
            {.image = gbuffer.albedoRoughness.get(), .view = gbuffer.albedoRoughness.getView({})},
            {.image = gbuffer.normalMaterial.get(), .view = gbuffer.normalMaterial.getView({})},
        },
        {.image = depth.get(), .view = depth.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad},
        BarrierBehavior::eSuppressBarriers
    );
    DrawScene(cmd, false);
}

void App::DrawLighting(vk::CommandBuffer cmd, vk::Image backbuffer, vk::ImageView backbufferView) {
    auto rect = vk::Rect2D{{0, 0}, {resolution.x, resolution.y}};
    TransitionImages(cmd, {
        ImageBarrier(depth, depthLoad, sampled),
        ImageBarrier(gbuffer.albedoRoughness, colorWrite, sampled),
        ImageBarrier(gbuffer.normalMaterial, colorWrite, sampled),
    });

    auto* surface = &gbuffer;
    if (decals.IsEnabled()) {
        TransitionImages(cmd, {
            ImageBarrier(decalBuffer.albedoRoughness, discarded, colorWrite),
            ImageBarrier(decalBuffer.normalMaterial, discarded, colorWrite),
        });
        {
            auto renderTargets = etna::RenderTargetState(
                cmd,
                rect,
                {
                    {
                        .image = decalBuffer.albedoRoughness.get(),
                        .view = decalBuffer.albedoRoughness.getView({}),
                        .loadOp = vk::AttachmentLoadOp::eDontCare,
                    },
                    {
                        .image = decalBuffer.normalMaterial.get(),
                        .view = decalBuffer.normalMaterial.getView({}),
                        .loadOp = vk::AttachmentLoadOp::eDontCare,
                    },
                },
                {},
                BarrierBehavior::eSuppressBarriers
            );
            decals.Draw(
                cmd,
                lighting.inverseViewProjection,
                gbuffer.albedoRoughness,
                gbuffer.normalMaterial,
                depth,
                gbufferSampler
            );
        }
        TransitionImages(cmd, {
            ImageBarrier(decalBuffer.albedoRoughness, colorWrite, sampled),
            ImageBarrier(decalBuffer.normalMaterial, colorWrite, sampled),
        });
        surface = &decalBuffer;
    }

    auto programInfo = etna::get_shader_program("deferred lighting");
    auto descriptorSet = etna::create_descriptor_set(
        programInfo.getDescriptorLayoutId(0),
        cmd,
        {
            etna::Binding{0, surface->albedoRoughness.genBinding(gbufferSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{1, surface->normalMaterial.genBinding(gbufferSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{2, depth.genBinding(gbufferSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{3, lightingBuffer.genBinding()},
            skybox.GetBinding(),
        },
        BarrierBehavior::eSuppressBarriers
    );
    auto renderTargets = etna::RenderTargetState(
        cmd,
        rect, {{
            .image = backbuffer,
            .view = backbufferView,
            .loadOp = vk::AttachmentLoadOp::eDontCare,
        }},
        {}
    );
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, lightingPipeline.getVkPipeline());
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        lightingPipeline.getVkPipelineLayout(),
        0,
        {descriptorSet.getVkSet()},
        {}
    );
    cmd.draw(3, 1, 0, 0);
}

void App::DrawFrame() {
    auto currentCmdBuf = commandManager->acquireNext();
    etna::begin_frame();
    auto nextSwapchainImage = vkWindow->acquireNext();

    if (nextSwapchainImage) {
        auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

        ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
        {
            auto bounds = std::array{rocks[0].GetBounds(), rocks[1].GetBounds()};
            auto visible = scatter->Generate(camera.position, pushConstants.viewProjection, rockSettings, bounds);
            for (std::size_t model = 0; model < rocks.size(); model++) {
                rocks[model].UploadInstances(currentCmdBuf, visible[model]);
            }
            lighting.inverseViewProjection = glm::inverse(pushConstants.viewProjection);
            lighting.cameraPosition = pushConstants.cameraPosition;
            lighting.sunDirectionIntensity = skybox.GetSunDirectionIntensity();
            lighting.sunColor = glm::vec4(skybox.GetSunColor(), 1.0f);
            auto projection = camera.projTm(static_cast<float>(resolution.x) / resolution.y);
            auto viewRotation = glm::mat4_cast(glm::conjugate(camera.rotation));
            lighting.inverseSkyViewProjection = glm::inverse(projection * viewRotation);
            etna::set_state(
                currentCmdBuf, lightingBuffer.get(), vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferWrite
            );
            etna::flush_barriers(currentCmdBuf);
            currentCmdBuf.updateBuffer<Lighting>(lightingBuffer.get(), 0, {lighting});
            etna::set_state(
                currentCmdBuf, lightingBuffer.get(), vk::PipelineStageFlagBits2::eFragmentShader,
                vk::AccessFlagBits2::eUniformRead
            );
            etna::flush_barriers(currentCmdBuf);

            DrawGeometry(currentCmdBuf);
            DrawLighting(currentCmdBuf, backbuffer, backbufferView);

            etna::set_state(
                currentCmdBuf, backbuffer, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eColorAttachmentOptimal, vk::ImageAspectFlagBits::eColor
            );
            etna::flush_barriers(currentCmdBuf);
            guiRenderer->render(
                currentCmdBuf, {{0, 0}, {resolution.x, resolution.y}}, backbuffer, backbufferView, ImGui::GetDrawData()
            );

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

    auto wait = vk::SemaphoreSubmitInfo{
        .semaphore = renderingDone,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    auto signal = vk::SemaphoreSubmitInfo{
        .semaphore = presentReady.get(),
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    auto submitInfo = vk::SubmitInfo2{
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal,
    };
    ETNA_CHECK_VK_RESULT(context.getQueue().submit2({submitInfo}, {}));

    return vkWindow->present(presentReady.get(), backbufferView);
}
