#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include <imgui.h>


template <typename L>
struct LightPassInfo;

template <>
struct LightPassInfo<FinitePointLight>
{
  static constexpr const char* SHADER_NAME = "finite_point_light";
  static constexpr std::size_t VERTICES_PER_INSTANCE = 36;
};

template <>
struct LightPassInfo<InfinitePointLight>
{
  static constexpr const char* SHADER_NAME = "infinite_point_light";
  static constexpr std::size_t VERTICES_PER_INSTANCE = 6;
};

template <>
struct LightPassInfo<DirectionalLight>
{
  static constexpr const char* SHADER_NAME = "directional_light";
  static constexpr std::size_t VERTICES_PER_INSTANCE = 6;
};

template <>
struct LightPassInfo<AmbientLight>
{
  static constexpr const char* SHADER_NAME = "ambient_light";
  static constexpr std::size_t VERTICES_PER_INSTANCE = 6;
};

WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()}
{
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  auto& ctx = etna::get_context();

  gBuffer.baseColor = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "base_color",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });
  gBuffer.normal = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "normal",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });
  gBuffer.emissive = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "emissive",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });
  gBuffer.occlusionMetallicRoughness = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "occlusion_metallic_roughness",
    .format = vk::Format::eR32G32B32A32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });
  gBuffer.depth = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage =
      vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});

  std::unique_ptr<etna::OneShotCmdMgr> oneShotCmdMgr = ctx.createOneShotCmdMgr();

  vk::CommandBuffer commandBuffer = oneShotCmdMgr->start();

  defaultBaseColorImage = etna::create_image_from_bytes(
    etna::Image::CreateInfo{
      .extent = vk::Extent3D{1, 1, 1},
      .name = "default_base_color",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    },
    commandBuffer,
    std::array<std::uint8_t, 4>{0xFF, 0xFF, 0xFF, 0xFF}.data());
  defaultMetallicRoughnessImage = etna::create_image_from_bytes(
    etna::Image::CreateInfo{
      .extent = vk::Extent3D{1, 1, 1},
      .name = "default_metallic_roughness",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    },
    commandBuffer,
    std::array<std::uint8_t, 4>{0x00, 0xFF, 0xFF, 0xFF}.data());
  defaultNormalImage = etna::create_image_from_bytes(
    etna::Image::CreateInfo{
      .extent = vk::Extent3D{1, 1, 1},
      .name = "default_normal",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    },
    commandBuffer,
    std::array<std::uint8_t, 4>{0x80, 0x80, 0xFF, 0xFF}.data());
  defaultOcclusionImage = etna::create_image_from_bytes(
    etna::Image::CreateInfo{
      .extent = vk::Extent3D{1, 1, 1},
      .name = "default_occlusion",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    },
    commandBuffer,
    std::array<std::uint8_t, 4>{0xFF, 0xFF, 0xFF, 0xFF}.data());
  defaultEmissiveImage = etna::create_image_from_bytes(
    etna::Image::CreateInfo{
      .extent = vk::Extent3D{1, 1, 1},
      .name = "default_emissive",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    },
    commandBuffer,
    std::array<std::uint8_t, 4>{0x00, 0x00, 0x00, 0x00}.data());

  oneShotCmdMgr->submitAndWait(commandBuffer);
}

void WorldRenderer::loadScene(std::filesystem::path path)
{
  sceneMgr->selectScene(path);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "g_buffer_pass",
    {
      TASK1_SHADERS_ROOT "g_buffer_pass.vert.spv",
      TASK1_SHADERS_ROOT "g_buffer_pass.frag.spv",
    });

  for_each_known_light_type([]<std::size_t, typename L>() {
    etna::create_program(
      fmt::format("{}_pass", LightPassInfo<L>::SHADER_NAME).c_str(),
      {
        fmt::format("{}{}_pass.vert.spv", TASK1_SHADERS_ROOT, LightPassInfo<L>::SHADER_NAME),
        fmt::format("{}{}_pass.frag.spv", TASK1_SHADERS_ROOT, LightPassInfo<L>::SHADER_NAME),
      });
  });

  etna::create_program(
    "emissive_light_pass",
    {
      TASK1_SHADERS_ROOT "emissive_light_pass.vert.spv",
      TASK1_SHADERS_ROOT "emissive_light_pass.frag.spv",
    });
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  initQuadRenderer(swapchain_format);

  initGridRenderer(swapchain_format);

  initGBufferPipeline();

  initLightPassPipelines(swapchain_format);
}

void WorldRenderer::debugInput(const Keyboard& kb)
{
  if (kb[KeyboardKey::kG] == ButtonState::Falling)
  {
    drawDebugGrid = !drawDebugGrid;
  }
}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    viewProjection = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
  }
}

etna::DescriptorSet WorldRenderer::createMaterialBindings(
  vk::CommandBuffer command_buffer, std::optional<std::size_t> material_index) const
{
  auto createImageBinding =
    [this](std::optional<std::size_t> texture_index, const etna::Image& fallback_image) {
      const etna::Image* image;
      const etna::Sampler* sampler;
      if (texture_index.has_value())
      {
        const SceneMaterials& materials = sceneMgr->getMaterials();

        const Texture& texture = materials.getTexture(*texture_index);
        image = &materials.getImage(texture.image);
        sampler = &materials.getSampler(texture.sampler);
      }
      else
      {
        image = &fallback_image;
        sampler = &defaultSampler;
      }

      return image->genBinding(sampler->get(), vk::ImageLayout::eShaderReadOnlyOptimal);
    };

  std::array<std::optional<std::size_t>, 5> textureIndices;

  if (material_index.has_value())
  {
    const SceneMaterials& materials = sceneMgr->getMaterials();

    const Material& material = materials.getMaterial(*material_index);

    textureIndices[0] = material.baseColorTexture;
    textureIndices[1] = material.metallicRoughnessTexture;
    textureIndices[2] = material.normalTexture;
    textureIndices[3] = material.occlusionTexture;
    textureIndices[4] = material.emissiveTexture;
  }

  std::array<const etna::Image*, 5> kFallbackImages = {
    &defaultBaseColorImage,
    &defaultMetallicRoughnessImage,
    &defaultNormalImage,
    &defaultOcclusionImage,
    &defaultEmissiveImage,
  };

  std::vector<etna::ImageBinding> imageBindings;
  for (std::size_t i = 0; i < textureIndices.size(); ++i)
  {
    imageBindings.push_back(createImageBinding(textureIndices[i], *kFallbackImages[i]));
  }

  for (const etna::ImageBinding& binding : imageBindings)
  {
    etna::set_state(
      command_buffer,
      binding.image.get(),
      vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2::eShaderRead,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor);
  }

  std::vector<etna::Binding> bindings;
  for (std::size_t i = 0; i < textureIndices.size(); ++i)
  {
    bindings.push_back(etna::Binding{static_cast<std::uint32_t>(i), imageBindings[i]});
  }

  auto gBufferPassInfo = etna::get_shader_program("g_buffer_pass");

  return etna::create_descriptor_set(
    gBufferPassInfo.getDescriptorLayoutId(0), command_buffer, std::move(bindings));
}

WorldRenderer::MaterialConstants WorldRenderer::getMaterialConstants(
  std::optional<std::size_t> material_index) const
{
  glm::vec4 baseColorMetallicFactor = glm::vec4(1.0, 1.0, 1.0, 1.0);
  glm::vec4 emissiveRoughnessFactors = glm::vec4(0.0, 0.0, 0.0, 1.0);

  if (material_index.has_value())
  {
    const SceneMaterials& materials = sceneMgr->getMaterials();

    const Material& material = materials.getMaterial(*material_index);

    baseColorMetallicFactor = glm::vec4(glm::vec3(material.baseColor), material.metallicFactor);
    emissiveRoughnessFactors =
      glm::vec4(glm::vec3(material.emissiveFactor), material.roughnessFactor);
  }

  return MaterialConstants{
    .baseColorMetallicFactor = baseColorMetallicFactor,
    .emissiveRoughnessFactors = emissiveRoughnessFactors,
  };
}

void WorldRenderer::doGBufferPass(vk::CommandBuffer command_buffer)
{
  etna::RenderTargetState renderTargets(
    command_buffer,
    {{0, 0}, {resolution.x, resolution.y}},
    {
      {.image = gBuffer.baseColor.get(), .view = gBuffer.baseColor.getView({})},
      {.image = gBuffer.normal.get(), .view = gBuffer.normal.getView({})},
      {.image = gBuffer.emissive.get(), .view = gBuffer.emissive.getView({})},
      {.image = gBuffer.occlusionMetallicRoughness.get(),
       .view = gBuffer.occlusionMetallicRoughness.getView({})},
    },
    {.image = gBuffer.depth.get(), .view = gBuffer.depth.getView({})});

  command_buffer.bindPipeline(
    vk::PipelineBindPoint::eGraphics, gBufferPassPipeline.getVkPipeline());

  struct Constants
  {
    glm::mat4 viewProjection;
    glm::mat4 model;
    MaterialConstants materialConstants;
  };

  const SceneMeshes& sceneMeshes = sceneMgr->getMeshes();

  if (!sceneMeshes.getVertexBuffer())
  {
    return;
  }

  command_buffer.bindVertexBuffers(0, {sceneMeshes.getVertexBuffer()}, {0});
  command_buffer.bindIndexBuffer(sceneMeshes.getIndexBuffer(), 0, vk::IndexType::eUint32);

  Constants pushConstants = {
    .viewProjection = viewProjection,
    .model = glm::mat4(1.0),
    .materialConstants = {},
  };

  auto instanceMatrices = sceneMgr->getInstanceMatrices();

  auto meshInstances = sceneMeshes.getInstanceMeshes();

  auto meshes = sceneMeshes.getMeshes();
  auto renderElements = sceneMeshes.getRenderElements();

  for (std::size_t instIdx = 0; instIdx < meshInstances.size(); ++instIdx)
  {
    pushConstants.model = instanceMatrices[instIdx];

    const auto meshInstance = meshInstances[instIdx];

    for (std::size_t j = 0; j < meshes[meshInstance.mesh].relemCount; ++j)
    {
      const auto relemIdx = meshes[meshInstance.mesh].firstRelem + j;
      const auto& relem = renderElements[relemIdx];

      pushConstants.materialConstants = getMaterialConstants(relem.material);

      auto set = createMaterialBindings(command_buffer, relem.material);

      command_buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        gBufferPassPipeline.getVkPipelineLayout(),
        0,
        {set.getVkSet()},
        {});
      command_buffer.pushConstants<Constants>(
        gBufferPassPipeline.getVkPipelineLayout(),
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        0,
        {pushConstants});

      command_buffer.drawIndexed(relem.indexCount, 1, relem.indexOffset, relem.vertexOffset, 0);
    }
  }
}

void WorldRenderer::doLightingPasses(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  for (const etna::Image* image : {
         &gBuffer.baseColor,
         &gBuffer.normal,
         &gBuffer.emissive,
         &gBuffer.occlusionMetallicRoughness,
       })
  {
    etna::set_state(
      cmd_buf,
      image->get(),
      vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2::eShaderRead,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor);
  }
  etna::set_state(
    cmd_buf,
    gBuffer.depth.get(),
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::AccessFlagBits2::eShaderRead,
    vk::ImageLayout::eShaderReadOnlyOptimal,
    vk::ImageAspectFlagBits::eDepth);

  etna::RenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {resolution.x, resolution.y}},
    {
      {.image = target_image, .view = target_image_view},
    },
    {});

  sceneMgr->getLights().forEachKnownLightType([this, cmd_buf]<std::size_t I, typename L>(
                                                const SceneLights::HomogenousLightBuffer& buffer) {
    if (buffer.count == 0)
    {
      return;
    }

    auto lightPassInfo =
      etna::get_shader_program(fmt::format("{}_pass", LightPassInfo<L>::SHADER_NAME).c_str());

    auto set = etna::create_descriptor_set(
      lightPassInfo.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, buffer.buffer.genBinding()},
        etna::Binding{
          1,
          gBuffer.baseColor.genBinding(
            defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{
          2,
          gBuffer.normal.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{
          3,
          gBuffer.emissive.genBinding(
            defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{
          4,
          gBuffer.occlusionMetallicRoughness.genBinding(
            defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{
          5,
          gBuffer.depth.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      });

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, lightPassPipelines[I].getVkPipeline());
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      lightPassPipelines[I].getVkPipelineLayout(),
      0,
      {set.getVkSet()},
      {});

    cmd_buf.pushConstants<glm::mat4>(
      lightPassPipelines[I].getVkPipelineLayout(),
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
      0,
      {viewProjection});

    cmd_buf.draw(LightPassInfo<L>::VERTICES_PER_INSTANCE, buffer.count, 0, 0);
  });

  auto lightPassInfo = etna::get_shader_program("emissive_light_pass");

  auto set = etna::create_descriptor_set(
    lightPassInfo.getDescriptorLayoutId(0),
    cmd_buf,
    {
      etna::Binding{
        0,
        gBuffer.emissive.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, emissiveLightPassPipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    emissiveLightPassPipeline.getVkPipelineLayout(),
    0,
    {set.getVkSet()},
    {});

  cmd_buf.draw(6, 1, 0, 0);
}

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  // G-Buffer pass

  {
    ETNA_PROFILE_GPU(cmd_buf, gBufferPass);

    doGBufferPass(cmd_buf);
  }

  // Lighting passes

  {
    ETNA_PROFILE_GPU(cmd_buf, lightingPasses);

    doLightingPasses(cmd_buf, target_image, target_image_view);
  }

  // Grid pass

  if (drawDebugGrid)
  {
    gridRenderer->render(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      viewProjection,
      target_image,
      target_image_view,
      gBuffer.depth.get(),
      gBuffer.depth.getView({}));
  }

  // Debug quad pass

  if (debugQuadMode != DebugQuadMode::kDisabled)
  {
    const etna::Image* image = nullptr;
    switch (debugQuadMode)
    {
    case DebugQuadMode::kBaseColor:
      image = &gBuffer.baseColor;
      break;
    case DebugQuadMode::kNormal:
      image = &gBuffer.normal;
      break;
    case DebugQuadMode::kEmissive:
      image = &gBuffer.emissive;
      break;
    case DebugQuadMode::kOcclusionMetallicRoughness:
      image = &gBuffer.occlusionMetallicRoughness;
      break;
    case DebugQuadMode::kDepth:
      image = &gBuffer.depth;
      break;
    default:
      break;
    }

    quadRenderer->render(cmd_buf, target_image, target_image_view, *image, defaultSampler);
  }
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Simple render settings");

  static std::array<std::pair<DebugQuadMode, std::string>, 6> kAvailableQuadDebugModes = {{
    {DebugQuadMode::kDisabled, "Disabled"},
    {DebugQuadMode::kBaseColor, "Base color"},
    {DebugQuadMode::kNormal, "Normal"},
    {DebugQuadMode::kEmissive, "Emissive"},
    {DebugQuadMode::kOcclusionMetallicRoughness, "Occlusion, metallic, roughness"},
    {DebugQuadMode::kDepth, "Depth"},
  }};

  const char* label =
    std::ranges::find(kAvailableQuadDebugModes, debugQuadMode, [](const auto& mode_name) {
      return mode_name.first;
    })->second.c_str();

  if (ImGui::BeginCombo("Debug quad mode", label))
  {
    for (auto [mode, name] : kAvailableQuadDebugModes)
    {
      if (ImGui::Selectable(name.c_str()))
      {
        debugQuadMode = mode;
      }
    }

    ImGui::EndCombo();
  }

  ImGui::Checkbox("Draw grid", &drawDebugGrid);

  ImGui::Text(
    "Application average %.3f ms/frame (%.1f FPS)",
    1000.0f / ImGui::GetIO().Framerate,
    ImGui::GetIO().Framerate);

  ImGui::NewLine();

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press 'B' to recompile and reload shaders");
  ImGui::End();
}


void WorldRenderer::initGBufferPipeline()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getMeshes().getVertexFormatDescription(),
    }},
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  gBufferPassPipeline =
    pipelineManager
      .createGraphicsPipeline(
        "g_buffer_pass",
        etna::GraphicsPipeline::CreateInfo{
          .vertexShaderInput = sceneVertexInputDesc,
          .rasterizationConfig =
            vk::PipelineRasterizationStateCreateInfo{
              .polygonMode = vk::PolygonMode::eFill,
              .cullMode = vk::CullModeFlagBits::eBack,
              .frontFace = vk::FrontFace::eCounterClockwise,
              .lineWidth = 1.f,
            },
          .blendingConfig =
            etna::GraphicsPipeline::CreateInfo::Blending{
              .attachments =
                {
                  vk::PipelineColorBlendAttachmentState{
                    .blendEnable = vk::False,
                    .colorWriteMask = vk::ColorComponentFlagBits::eR |
                      vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                      vk::ColorComponentFlagBits::eA,
                  },
                  vk::PipelineColorBlendAttachmentState{
                    .blendEnable = vk::False,
                    .colorWriteMask = vk::ColorComponentFlagBits::eR |
                      vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                      vk::ColorComponentFlagBits::eA,
                  },
                  vk::PipelineColorBlendAttachmentState{
                    .blendEnable = vk::False,
                    .colorWriteMask = vk::ColorComponentFlagBits::eR |
                      vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                      vk::ColorComponentFlagBits::eA,
                  },
                  vk::PipelineColorBlendAttachmentState{
                    .blendEnable = vk::False,
                    .colorWriteMask = vk::ColorComponentFlagBits::eR |
                      vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                      vk::ColorComponentFlagBits::eA,
                  },
                },
              .logicOpEnable = false,
              .logicOp = vk::LogicOp::eCopy,
            },
          .fragmentShaderOutput =
            {
              .colorAttachmentFormats =
                {
                  vk::Format::eR32G32B32A32Sfloat,
                  vk::Format::eR32G32B32A32Sfloat,
                  vk::Format::eR32G32B32A32Sfloat,
                  vk::Format::eR32G32B32A32Sfloat,
                },
              .depthAttachmentFormat = vk::Format::eD32Sfloat,
            },
        });
}

void WorldRenderer::initLightPassPipelines(vk::Format swapchain_format)
{
  etna::GraphicsPipeline::CreateInfo lightPassPipelineCreateInfo =
    {
      .vertexShaderInput =
        etna::VertexShaderInputDescription{
          .bindings = {etna::VertexShaderInputDescription::Binding{
            .byteStreamDescription = etna::VertexByteStreamFormatDescription{}}},
        },
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eBack,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .blendingConfig =
        etna::GraphicsPipeline::CreateInfo::Blending{
          .attachments =
            {
              vk::PipelineColorBlendAttachmentState{
                .blendEnable = vk::True,
                .srcColorBlendFactor = vk::BlendFactor::eOne,
                .dstColorBlendFactor = vk::BlendFactor::eOne,
                .colorBlendOp = vk::BlendOp::eAdd,
                .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                .dstAlphaBlendFactor = vk::BlendFactor::eOne,
                .alphaBlendOp = vk::BlendOp::eAdd,
                .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
              },
            },
          .logicOpEnable = false,
          .logicOp = vk::LogicOp::eCopy,
        },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {swapchain_format},
        },
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  for_each_known_light_type(
    [this, &pipelineManager, &lightPassPipelineCreateInfo]<std::size_t I, typename L>() {
      lightPassPipelines[I] = pipelineManager.createGraphicsPipeline(
        fmt::format("{}_pass", LightPassInfo<L>::SHADER_NAME).c_str(), lightPassPipelineCreateInfo);
    });

  emissiveLightPassPipeline =
    pipelineManager.createGraphicsPipeline("emissive_light_pass", lightPassPipelineCreateInfo);
}

void WorldRenderer::initQuadRenderer(vk::Format swapchain_format)
{
  quadRenderer = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{
    .format = swapchain_format,
    .rect = {{0, 0}, {512, 512}},
  });
}

void WorldRenderer::initGridRenderer(vk::Format swapchain_format)
{
  std::vector<LineRenderer::Vertex> gridVertices;
  std::vector<std::uint32_t> gridIndices;

  const glm::vec3 gridExtent = glm::vec3(4.0);
  const int gridFrequency = 2;

  // Axes

  gridVertices.push_back({glm::vec3(0.0, 0.0, 0.0), glm::vec3(1.0, 0.0, 0.0)});
  gridVertices.push_back({glm::vec3(gridExtent.x, 0.0, 0.0), glm::vec3(1.0, 0.0, 0.0)});
  gridVertices.push_back({glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0)});
  gridVertices.push_back({glm::vec3(0.0, gridExtent.y, 0.0), glm::vec3(0.0, 1.0, 0.0)});
  gridVertices.push_back({glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 0.0, 1.0)});
  gridVertices.push_back({glm::vec3(0.0, 0.0, gridExtent.z), glm::vec3(0.0, 0.0, 1.0)});

  gridIndices.push_back(0);
  gridIndices.push_back(1);
  gridIndices.push_back(2);
  gridIndices.push_back(3);
  gridIndices.push_back(4);
  gridIndices.push_back(5);

  // Lines

  for (int lineDir = 0; lineDir < 3; ++lineDir)
  {
    for (int sweepDir = 0; sweepDir < 3; ++sweepDir)
    {
      if (sweepDir == lineDir)
      {
        continue;
      }

      int bound = static_cast<int>(gridExtent[sweepDir] * gridFrequency);
      for (int i = -bound; i <= bound; ++i)
      {
        glm::vec3 color = i % gridFrequency == 0 ? glm::vec3(0.3) : glm::vec3(0.1);

        glm::vec3 position1 = glm::vec3(0.0);
        position1[lineDir] = -gridExtent[lineDir];
        position1[sweepDir] = i / static_cast<float>(gridFrequency);

        gridIndices.push_back(gridVertices.size());
        gridVertices.push_back({position1, color});

        glm::vec3 position2 = glm::vec3(0.0);
        if (i != 0)
        {
          position2[lineDir] = gridExtent[lineDir];
        }
        position2[sweepDir] = i / static_cast<float>(gridFrequency);

        gridIndices.push_back(gridVertices.size());
        gridVertices.push_back({position2, color});
      }
    }
  }

  gridRenderer = std::make_unique<LineRenderer>(LineRenderer::CreateInfo{
    .format = swapchain_format,
    .vertices = std::move(gridVertices),
    .indices = std::move(gridIndices),
  });
}
