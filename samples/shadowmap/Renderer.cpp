#include "Renderer.h"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void Renderer::allocateResources()
{
  mainViewDepth = context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "main_view_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  shadowMap = context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "shadow_map",
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  constants = context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "constants"
  });

  constants.map();
}

void Renderer::loadScene(std::filesystem::path path)
{
  sceneMgr->selectScene(path);

  // TODO: Make a separate stage
  loadShaders();
  preparePipelines();
}


/// PIPELINES CREATION

void Renderer::preparePipelines()
{
  // create full screen quad for debug purposes
  //
  quadRenderer = std::make_unique<QuadRenderer>(QuadRenderer::CreateInfo{
      .format = window->getCurrentFormat(),
      .rect = { {0, 0}, {512, 512} },
    });
  setupPipelines();
}

void Renderer::loadShaders()
{
  etna::create_program("simple_material",
    {GRAPHICS_COURSE_ROOT"/resources/shaders/simple_shadow.frag.spv", GRAPHICS_COURSE_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow", {GRAPHICS_COURSE_ROOT"/resources/shaders/simple.vert.spv"});
}

void Renderer::setupPipelines()
{
  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = sceneMgr->getVertexFormatDescription(),
        }},
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = sceneVertexInputDesc,
      .rasterizationConfig = vk::PipelineRasterizationStateCreateInfo{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.f,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {window->getCurrentFormat()},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });
  shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = sceneVertexInputDesc,
      .rasterizationConfig = vk::PipelineRasterizationStateCreateInfo{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.f,
      },
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm,
        },
    });
}


/// COMMAND BUFFER FILLING

void Renderer::renderScene(vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout)
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

    cmd_buf.pushConstants<PushConstants>(pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, {pushConst2M});

    const auto meshIdx = instanceMeshes[instIdx];

    for (std::size_t j = 0; j < meshes[meshIdx].relemCount; ++j)
    {
      const auto relemIdx = meshes[meshIdx].firstRelem + j;
      const auto &relem = relems[relemIdx];
      cmd_buf.drawIndexed(relem.indexCount, 1, relem.indexOffset, relem.vertexOffset, 0);
    }
  }
}

void Renderer::renderWorld(vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  // draw scene to shadowmap

  {
    etna::RenderTargetState renderTargets(cmd_buf, {{0, 0}, {2048, 2048}}, {}, {.image = shadowMap.get(), .view = shadowMap.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, shadowPipeline.getVkPipeline());
    renderScene(cmd_buf, lightMatrix, shadowPipeline.getVkPipelineLayout());
  }

  // draw final scene to screen

  {
    auto simpleMaterialInfo = etna::get_shader_program("simple_material");

    auto set = etna::create_descriptor_set(simpleMaterialInfo.getDescriptorLayoutId(0), cmd_buf,
    {
      etna::Binding {0, constants.genBinding()},
      etna::Binding {1, shadowMap.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });

    etna::RenderTargetState renderTargets(cmd_buf, {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, basicForwardPipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, basicForwardPipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});

    renderScene(cmd_buf, worldViewProj, basicForwardPipeline.getVkPipelineLayout());
  }

  if(drawDebugFSQuad)
    quadRenderer->render(cmd_buf, target_image, target_image_view, shadowMap, defaultSampler);
}
