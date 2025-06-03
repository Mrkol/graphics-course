#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>


PlaceholderTextureManager::PlaceholderTextureManager(vk::CommandBuffer cmd_buf) {
  unsigned char white_img[4] = {255, 255, 255, 255};
  for (int i = 0; i < 32; ++i) {
    textures.emplace_back(
      std::move(etna::create_image_from_bytes(etna::Image::CreateInfo{
        .extent = {1, 1, 1},
        .name = "placeholder_img",
      },
      cmd_buf,
      white_img)
    ));
  }
}


WorldRenderer::WorldRenderer()
  : oneShotCommands{etna::get_context().createOneShotCmdMgr()}
  , transferHelper{etna::BlockingTransferHelper::CreateInfo{.stagingSize = 65536 * 4}}  // Not sure about size. Probably this is enough.
  , sceneMgr{std::make_unique<SceneManager>()}
  , placeholderTextureManager{oneShotCommands->start()}
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

  zeroLengthBuffer = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = 1,  // I don't know why Vulkan forbids 0-length buffers.
    .bufferUsage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "zero_length_buffer",
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo
  {
    .filter = vk::Filter::eLinear,
    .addressMode = vk::SamplerAddressMode::eRepeat,
    .name = "default_sampler",
  });
}

void WorldRenderer::loadScene(std::filesystem::path path)
{
  //sceneMgr->selectScene(path);
  sceneMgr->selectSceneCompressed(path);
  texturesDirty = true;
}

void WorldRenderer::unbindScene(vk::CommandBuffer cmd_buf) {
  cmd_buf.bindVertexBuffers(0, {zeroLengthBuffer.get()}, {0});
  cmd_buf.bindIndexBuffer(zeroLengthBuffer.get(), 0, vk::IndexType::eUint32);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "static_mesh_material",
    {PARTICLES_SHADERS_ROOT "static_mesh.frag.spv",
      PARTICLES_SHADERS_ROOT "static_mesh.vert.spv"});
  etna::create_program("static_mesh", {PARTICLES_SHADERS_ROOT "static_mesh.vert.spv"});
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
    }},
  };

  auto& pipelineManager = etna::get_context().getPipelineManager();

  staticMeshPipeline = {};
  staticMeshPipeline = pipelineManager.createGraphicsPipeline(
    "static_mesh_material",
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
        {
          .colorAttachmentFormats = {swapchain_format},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });
}

void WorldRenderer::debugInput(const Keyboard&) {}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
  }
}

void WorldRenderer::refresh_textures(vk::CommandBuffer cmd_buf) {
  auto images = sceneMgr->getImages();

  if (texturesDirty) {
    texturesDirty = false;

    relemToTextureMap = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
      .size = sizeof(int32_t) * sceneMgr->getRenderElements().size(),
      .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
      .name = "relem_to_texture_map",
    });

    relemToTextureMapCPU.clear();
    relemToTextureMapCPU.reserve(sceneMgr->getRenderElements().size());
    for (auto& relem : sceneMgr->getRenderElements()) {
      if (relem.material.albedoId == Material::ImageId::Invalid) {
        relemToTextureMapCPU.push_back(static_cast<uint32_t>(images.size() - 1));
      } else {
        relemToTextureMapCPU.push_back(static_cast<uint32_t>(relem.material.albedoId));
      }
    }
    transferHelper.uploadBuffer<int>(*oneShotCommands, relemToTextureMap, 0, relemToTextureMapCPU);

    bindings.clear();
    bindings.reserve(images.size());
    for (auto& img : images) {
      bindings.push_back(
        etna::Binding{
          0,
          img.genBinding(
                          defaultSampler.get(),
                          vk::ImageLayout::eShaderReadOnlyOptimal
                        ),
          static_cast<uint32_t>(bindings.size())
        }
      );
    }

    size_t commands_amount = 0;
    {
      auto instanceMeshes = sceneMgr->getInstanceMeshes();
      auto meshes = sceneMgr->getMeshes();
      for (std::size_t instIdx = 0; instIdx < instanceMeshes.size(); ++instIdx)
      {
        commands_amount += meshes[instanceMeshes[instIdx]].relemCount;
      }
    }

    etna::DescriptorSetInfo texArrayInfo;
    texArrayInfo.addResource({
      .binding=0,
      .descriptorType=vk::DescriptorType::eCombinedImageSampler,
      // .descriptorCount=static_cast<uint32_t>(images.size()),  // This infuriates vulkan, because he expects 32 textures (he looked at the shader probably).
      .descriptorCount=static_cast<uint32_t>(32),
      .stageFlags=vk::ShaderStageFlagBits::eFragment,
      .pImmutableSamplers=nullptr
    });

    etna::DescriptorSetInfo relemMapBufInfo;
    relemMapBufInfo.addResource({
      .binding=0,
      .descriptorType=vk::DescriptorType::eStorageBuffer,
      // .descriptorCount=static_cast<uint32_t>(sceneMgr->getRenderElements().size()),
      .descriptorCount=static_cast<uint32_t>(1),
      .stageFlags=vk::ShaderStageFlagBits::eFragment,
      .pImmutableSamplers=nullptr
    });

    etna::DescriptorSetInfo drawParamsBufInfo;
    drawParamsBufInfo.addResource({
      .binding=0,
      .descriptorType=vk::DescriptorType::eStorageBuffer,
      // .descriptorCount=static_cast<uint32_t>(commands_amount),
      .descriptorCount=static_cast<uint32_t>(1),
      .stageFlags=vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
      .pImmutableSamplers=nullptr
    });
    etna::get_context().getDescriptorSetLayouts().clear(etna::get_context().getDevice());
    etna::get_context().getDescriptorSetLayouts().registerLayout(etna::get_context().getDevice(), texArrayInfo);
    etna::get_context().getDescriptorSetLayouts().registerLayout(etna::get_context().getDevice(), relemMapBufInfo);
    etna::get_context().getDescriptorSetLayouts().registerLayout(etna::get_context().getDevice(), drawParamsBufInfo);

    drawCommandsBuffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
      .size = sizeof(vk::DrawIndexedIndirectCommand) * commands_amount,
      .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndirectBuffer,
      .name = "draw_commands_buffer"
    });

    drawParamsBuffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
      .size = sizeof(DrawParams) * commands_amount,
      .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
      .name = "draw_params_buffer"
    });

    texturesDescriptorSet = etna::create_persistent_descriptor_set(
      etna::get_shader_program("static_mesh_material").getDescriptorLayoutId(0),
      bindings,
      true
    );
  }

  relemToTextureMapDescriptorSet = etna::create_descriptor_set(
    etna::get_shader_program("static_mesh_material").getDescriptorLayoutId(1),
    cmd_buf,
    {
      etna::Binding{0, relemToTextureMap.genBinding()}
    }
  );

  drawParamsDescriptorSet = etna::create_descriptor_set(
    etna::get_shader_program("static_mesh_material").getDescriptorLayoutId(2),
    cmd_buf,
    {
      etna::Binding{0, drawParamsBuffer.genBinding()}
    }
  );

  assert(relemToTextureMapDescriptorSet.isValid());
  assert(texturesDescriptorSet.isValid());
  assert(drawParamsDescriptorSet.isValid());

  {
    vk::DescriptorSet vkSets[3];
    vkSets[0] = texturesDescriptorSet.getVkSet();
    vkSets[1] = relemToTextureMapDescriptorSet.getVkSet();
    vkSets[2] = drawParamsDescriptorSet.getVkSet();
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipelineLayout(), 0, 3, vkSets, 0, nullptr);
  }

  for (auto& img : images) {
    etna::set_state(
      cmd_buf,
      img.get(),
      vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2::eColorAttachmentRead,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::ImageAspectFlagBits::eColor);
  }
  etna::flush_barriers(cmd_buf);
}

void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, [[maybe_unused]] vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer())
    return;

  cmd_buf.bindVertexBuffers(0, {sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);

  pushConsts.projView = glob_tm;
  cmd_buf.pushConstants<PushConstants>(
    pipeline_layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, {pushConsts});

  auto instanceMeshes = sceneMgr->getInstanceMeshes();
  auto instanceMatrices = sceneMgr->getInstanceMatrices();

  auto meshes = sceneMgr->getMeshes();
  auto relems = sceneMgr->getRenderElements();
  /*
  auto images = sceneMgr->getImages();

  std::vector<etna::DescriptorSet> texturesDescriptorSets;
  for (auto& img : images) {
    texturesDescriptorSets.push_back(etna::create_descriptor_set(
      etna::get_shader_program("static_mesh_material").getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, img.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
      }
    ));
  }
  */

  drawCommands.clear();
  drawParams.clear();

  for (std::size_t instIdx = 0; instIdx < instanceMeshes.size(); ++instIdx)
  {
    // pushConst2M.model = instanceMatrices[instIdx];

    const auto meshIdx = instanceMeshes[instIdx];

    for (std::size_t j = 0; j < meshes[meshIdx].relemCount; ++j)
    {
      const auto relemIdx = meshes[meshIdx].firstRelem + j;
      const auto& relem = relems[relemIdx];
      // auto& img = relem.material.albedoId == Material::ImageId::Invalid ? images.back() : images[static_cast<uint32_t>(relem.material.albedoId)];
      // auto& set = relem.material.albedoId == Material::ImageId::Invalid ? texturesDescriptorSets.back() : texturesDescriptorSets[static_cast<uint32_t>(relem.material.albedoId)];

      // {
      //   vk::DescriptorSet vkSet = set.getVkSet();
      //   cmd_buf.bindDescriptorSets(
      //     vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
      // }

      // pushConst2M.relemIdx = static_cast<int32_t>(relemIdx);

      drawParams.push_back({
        .model = instanceMatrices[instIdx],
        .relemIdx = static_cast<int32_t>(relemIdx)
      });
      
      // cmd_buf.drawIndexed(relem.indexCount, 1, relem.indexOffset, relem.vertexOffset, 0);
      drawCommands.push_back(vk::DrawIndexedIndirectCommand{
        relem.indexCount,
        1,
        relem.indexOffset,
        static_cast<int32_t>(relem.vertexOffset),
        0
      });
    }
  }

  transferHelper.uploadBuffer<vk::DrawIndexedIndirectCommand>(*oneShotCommands, drawCommandsBuffer, 0, drawCommands);
  transferHelper.uploadBuffer<DrawParams>(*oneShotCommands, drawParamsBuffer, 0, drawParams);

  cmd_buf.drawIndexedIndirect(
    drawCommandsBuffer.get(),
    0,
    static_cast<uint32_t>(drawCommands.size()),
    sizeof(vk::DrawIndexedIndirectCommand)
  );
}

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  // draw final scene to screen
  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    refresh_textures(cmd_buf);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }
}
