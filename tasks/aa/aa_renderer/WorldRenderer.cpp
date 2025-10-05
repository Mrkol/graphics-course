#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include <imgui.h>


WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()}
{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  mainViewDepth = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });

  mainColor = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_color_offscreen",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment
                | vk::ImageUsageFlagBits::eSampled
                | vk::ImageUsageFlagBits::eTransferSrc
  });

  const uint32_t shRes = (settings_.shadowResIndex == 0 ? 1024 :
                          settings_.shadowResIndex == 2 ? 4096 : 2048);

  shadowMap = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{shRes, shRes, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
                | vk::ImageUsageFlagBits::eSampled,
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});

  {
    etna::Sampler::CreateInfo si{};
    si.name = "post_sampler";
    si.addressMode = vk::SamplerAddressMode::eClampToEdge;
    postSampler = etna::Sampler(si);
  }

  constants = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants",
  });
}

void WorldRenderer::loadScene(std::filesystem::path path)
{
  sceneMgr->selectScene(path);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "simple_material",
    { AA_RENDERER_SHADERS_ROOT "simple_shadow.frag.spv",
      AA_RENDERER_SHADERS_ROOT "simple.vert.spv" });

  etna::create_program("simple_shadow",
    { AA_RENDERER_SHADERS_ROOT "simple.vert.spv" });

  etna::create_program("fxaa",
    { AA_RENDERER_SHADERS_ROOT "simple_fs.vert.spv",
      AA_RENDERER_SHADERS_ROOT "fxaa.frag.spv" });
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  quadRenderer = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{
    .format = swapchain_format,
    .rect = {{0, 0}, {512, 512}},
  });

  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
    }},
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  basicForwardPipeline = {};
  basicForwardPipeline = pipelineManager.createGraphicsPipeline(
    "simple_material",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = sceneVertexInputDesc,
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eBack,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .fragmentShaderOutput =
        { .colorAttachmentFormats = { swapchain_format },
          .depthAttachmentFormat  = vk::Format::eD32Sfloat },
    });

  basicForwardOffscreenPipeline = {};
  basicForwardOffscreenPipeline = pipelineManager.createGraphicsPipeline(
    "simple_material",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = sceneVertexInputDesc,
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eBack,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .fragmentShaderOutput =
        { .colorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
          .depthAttachmentFormat  = vk::Format::eD32Sfloat },
    });

  fxaaPipeline = {};
  fxaaPipeline = pipelineManager.createGraphicsPipeline(
    "fxaa",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = {},
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eNone,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .fragmentShaderOutput =
        { .colorAttachmentFormats = { swapchain_format },
          .depthAttachmentFormat  = vk::Format::eUndefined },
    });

  shadowPipeline = {};
  shadowPipeline = pipelineManager.createGraphicsPipeline(
    "simple_shadow",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = sceneVertexInputDesc,
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eBack,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .fragmentShaderOutput =
        { .colorAttachmentFormats = {},
          .depthAttachmentFormat  = vk::Format::eD16Unorm },
    });
}

void WorldRenderer::saveScreenshot()
{
    spdlog::info("saveScreenshot(): not implemented yet");
}

void WorldRenderer::debugInput(const Keyboard& kb)
{
  if (kb[KeyboardKey::kQ] == ButtonState::Falling)
    drawDebugFSQuad = !drawDebugFSQuad;

  if (kb[KeyboardKey::kP] == ButtonState::Falling)
    lightProps.usePerspectiveM = !lightProps.usePerspectiveM;
}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
  }

  {
    lightProps.radius          = std::max(lightProps.radius,          0.1f);
    lightProps.lightTargetDist = std::max(lightProps.lightTargetDist, 1.0f);

    const glm::mat4 V = glm::lookAtLH(
        lightPos,
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    const float fovDeg = 60.0f;
    const float nearZ  = 0.1f;
    const float farZ   = std::max(lightProps.lightTargetDist * 2.0f, nearZ + 0.1f);

    const glm::mat4 P = lightProps.usePerspectiveM
        ? glm::perspectiveLH_ZO(glm::radians(fovDeg), 1.0f, nearZ, farZ)
        : glm::orthoLH_ZO(-lightProps.radius, +lightProps.radius,
                          -lightProps.radius, +lightProps.radius,
                          nearZ, lightProps.lightTargetDist);

    lightMatrix = P * V;
  }

  {
    uniformParams.lightMatrix = lightMatrix;
    uniformParams.lightPos   = shader_vec4{ lightPos.x, lightPos.y, lightPos.z, lightIntensity_ };
    uniformParams.lightColor = shader_vec3{ lightColor_.x, lightColor_.y, lightColor_.z };
    uniformParams.time       = packet.currentTime * settings_.timeScale;
    uniformParams.baseColor  = shader_vec3{ uniformParams.baseColor.x,
                                            uniformParams.baseColor.y,
                                            uniformParams.baseColor.z };
    uniformParams.gamma      = settings_.gamma;
    uniformParams.exposure   = settings_.exposure;
    uniformParams.debugMode  = settings_.showNormals ? 1.0f : (settings_.baseColorOnly ? 2.0f : 0.0f);
    uniformParams.shadowBias = settings_.shadowBias;
    uniformParams.shadowPCF  = settings_.shadowPCF ? 1.0f : 0.0f;

    void* dst = constants.map();
    std::memcpy(dst, &uniformParams, sizeof(uniformParams));
    constants.unmap();
  }
}

void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer())
    return;

  cmd_buf.bindVertexBuffers(0, {sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);

  pushConst2M.projView = glob_tm;

  auto instanceMeshes = sceneMgr->getInstanceMeshes();
  auto instanceMatrices = sceneMgr->getInstanceMatrices();

  auto meshes = sceneMgr->getMeshes();
  auto relems = sceneMgr->getRenderElements();

  for (std::size_t instIdx = 0; instIdx < instanceMeshes.size(); ++instIdx)
  {
    pushConst2M.model = instanceMatrices[instIdx];

    cmd_buf.pushConstants<PushConstants>(
      pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, {pushConst2M});

    const auto meshIdx = instanceMeshes[instIdx];

    for (std::size_t j = 0; j < meshes[meshIdx].relemCount; ++j)
    {
      const auto relemIdx = meshes[meshIdx].firstRelem + j;
      const auto& relem = relems[relemIdx];
      cmd_buf.drawIndexed(relem.indexCount, 1, relem.indexOffset, relem.vertexOffset, 0);
    }
  }
}

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);
  {
    ETNA_PROFILE_GPU(cmd_buf, renderShadowMap);
    auto shExt = shadowMap.getExtent();
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {shExt.width, shExt.height}},
      {},
      {.image = shadowMap.get(), .view = shadowMap.getView({})},
      {});
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowPipeline.getVkPipeline());
    renderScene(cmd_buf, lightMatrix, shadowPipeline.getVkPipelineLayout());
  }

  {
    etna::set_state(
      cmd_buf,
      shadowMap.get(),
      vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eDepth
    );
    etna::flush_barriers(cmd_buf);
  }

  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    auto simpleMaterialInfo = etna::get_shader_program("simple_material");

    auto set = etna::create_descriptor_set(
      simpleMaterialInfo.getDescriptorLayoutId(0),
      cmd_buf,
      { etna::Binding{ 0, constants.genBinding() },
        etna::Binding{ 1, shadowMap.genBinding(defaultSampler.get(),
                           vk::ImageLayout::eShaderReadOnlyOptimal) }});

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0,0},{resolution.x, resolution.y}},
      {{ .image = mainColor.get(), .view = mainColor.getView({}) }},
      { .image = mainViewDepth.get(), .view = mainViewDepth.getView({}) },
      {});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, basicForwardOffscreenPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      basicForwardOffscreenPipeline.getVkPipelineLayout(),
      0,
      { set.getVkSet() },
      {});

    renderScene(cmd_buf, worldViewProj, basicForwardOffscreenPipeline.getVkPipelineLayout());
  }

  {
    etna::set_state(
      cmd_buf,
      mainColor.get(),
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd_buf);
  }

  {
    ETNA_PROFILE_GPU(cmd_buf, postprocessFXAA);
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0,0},{resolution.x, resolution.y}},
      {{ .image = target_image, .view = target_image_view }},
      {}, {});

    auto fxaaInfo = etna::get_shader_program("fxaa");
    auto set = etna::create_descriptor_set(
      fxaaInfo.getDescriptorLayoutId(0),
      cmd_buf,
      { etna::Binding{ 0, mainColor.genBinding(postSampler.get(),
                        vk::ImageLayout::eShaderReadOnlyOptimal) }});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, fxaaPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                              fxaaPipeline.getVkPipelineLayout(), 0, { set.getVkSet() }, {});

    struct PushPC {
        glm::vec4 params;
    };

    PushPC pc{
        glm::vec4(
            1.0f / float(resolution.x),
            1.0f / float(resolution.y),
            purpleTint_ ? 1.0f : 0.0f,
            (aaMode_ == AAMode::FXAA) ? 1.0f : 0.0f
        )
    };

    cmd_buf.pushConstants(
        fxaaPipeline.getVkPipelineLayout(),
        vk::ShaderStageFlagBits::eFragment,
        0, sizeof(PushPC), &pc
    );

    cmd_buf.draw(3, 1, 0, 0);
  }

  if (drawDebugFSQuad)
    quadRenderer->render(cmd_buf, target_image, target_image_view, shadowMap, defaultSampler);
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Simple render settings");

  const char* aaItems[] = { "None", "FXAA" };
  int aaIdx = (int)aaMode_;
  if (ImGui::Combo("AA", &aaIdx, aaItems, IM_ARRAYSIZE(aaItems))) {
    aaMode_ = (AAMode)aaIdx;
  }

  glm::vec3 base = { uniformParams.baseColor.r,
                     uniformParams.baseColor.g,
                     uniformParams.baseColor.b };
  if (ImGui::ColorEdit3("Meshes base color", &base.x,
        ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs))
  {
    uniformParams.baseColor = { base.r, base.g, base.b };
  }

  glm::vec3 lp = lightPos;
  if (ImGui::DragFloat3("Light source pos", &lp.x, 0.1f, -10.f, 10.f))
  {
    lightPos = lp;
    uniformParams.lightPos = { lp.x, lp.y, lp.z, lightIntensity_ };
  }

  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / (io.Framerate > 0 ? io.Framerate : 1.0f), io.Framerate);

  ImGui::NewLine();
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                     "Press 'B' to recompile and reload shaders");

  ImGui::End();
}

