#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>


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
}

void WorldRenderer::unbindScene(vk::CommandBuffer cmd_buf) {
  cmd_buf.bindVertexBuffers(0, {zeroLengthBuffer.get()}, {0});
  cmd_buf.bindIndexBuffer(zeroLengthBuffer.get(), 0, vk::IndexType::eUint32);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "static_mesh_material",
    {BINDLESS_SHADERS_ROOT "static_mesh.frag.spv",
      BINDLESS_SHADERS_ROOT "static_mesh.vert.spv"});
  etna::create_program("static_mesh", {BINDLESS_SHADERS_ROOT "static_mesh.vert.spv"});
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

void WorldRenderer::set_textures_states(vk::CommandBuffer cmd_buf) {
  auto images = sceneMgr->getImages();
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
      // auto& img = relem.material.albedoId == Material::ImageId::Invalid ? images.back() : images[static_cast<uint32_t>(relem.material.albedoId)];
      auto& set = relem.material.albedoId == Material::ImageId::Invalid ? texturesDescriptorSets.back() : texturesDescriptorSets[static_cast<uint32_t>(relem.material.albedoId)];

      {
        vk::DescriptorSet vkSet = set.getVkSet();
        cmd_buf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);
      }
      
      cmd_buf.drawIndexed(relem.indexCount, 1, relem.indexOffset, relem.vertexOffset, 0);
    }
  }
}

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  // draw final scene to screen
  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    set_textures_states(cmd_buf);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }
}
