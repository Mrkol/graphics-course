#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>

const std::size_t N_MAX_INSTANCES = 1 << 14;

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
    .name = "main_view_dept h",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });

  instanceMatricesBuf = ctx.createBuffer({
    .size = N_MAX_INSTANCES * sizeof(glm::mat4x4),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "instanceMatrices",
  });
  instanceMatricesBuf.map();
}

void WorldRenderer::loadScene(std::filesystem::path path)
{
  if (path.stem().string().ends_with("_baked"))
  {
    sceneMgr->selectSceneBaked(path);
  }
  else
  {
    sceneMgr->selectScene(path);
  }
  nInstances.assign(sceneMgr->getRenderElements().size(), 0);
}

void WorldRenderer::loadShaders()
{
  etna::create_program(
    "static_mesh_material",
    {MANY_OBJECTS_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     MANY_OBJECTS_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  etna::create_program("static_mesh", {MANY_OBJECTS_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
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

void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer())
    return;

  prepareFrame(glob_tm);

  auto staticMesh = etna::get_shader_program("static_mesh");

  cmd_buf.bindVertexBuffers(0, {sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);

  auto set = etna::create_descriptor_set(
    staticMesh.getDescriptorLayoutId(0),
    cmd_buf,
    {etna::Binding{0, instanceMatricesBuf.genBinding()}});

  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    staticMeshPipeline.getVkPipelineLayout(),
    0,
    {set.getVkSet()},
    {});


  pushConst2M.projView = glob_tm;

  auto relems = sceneMgr->getRenderElements();
  std::size_t firstInstance = 0;
  for (std::size_t j = 0; j < relems.size(); ++j)
  {
    if (nInstances[j] != 0)
    {
      const auto& relem = relems[j];
      cmd_buf.pushConstants<PushConstants>(
        pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, {pushConst2M});
      cmd_buf.drawIndexed(
        relem.indexCount, nInstances[j], relem.indexOffset, relem.vertexOffset, firstInstance);
      firstInstance += nInstances[j];
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

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = target_image, .view = target_image_view}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})});

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }
}

static bool IsNotVisble(const glm::mat2x3 bounds, const glm::mat4x4& transform)
{
  const glm::vec3 origin = transform * glm::vec4(bounds[0], 1.f);
  glm::mat3 oldExtent = glm::mat3(
    bounds[1].x, 0, 0,
    0, bounds[1].y, 0,
    0, 0, bounds[1].z
  );
  const glm::vec3 extent = glm::abs(glm::mat3(transform) * oldExtent) * glm::vec3(1.f, 1.f, 1.f);
  
  if(origin.z + extent.z < 0) return true;

  glm::vec3 lc = glm::vec3(origin) + extent;
  glm::vec3 rc = glm::vec3(origin) - extent;
  return 
        lc.x < -lc.z || 
        lc.y < -lc.z || 
        rc.x >  lc.z || 
        rc.y >  lc.z;
}

void WorldRenderer::prepareFrame(const glm::mat4x4& glob_tm)
{
  auto instanceMeshes = sceneMgr->getInstanceMeshes();
  auto instanceMatrices = sceneMgr->getInstanceMatrices();
  auto meshes = sceneMgr->getMeshes();
  auto bbs = sceneMgr->getBounds();

  auto* matrices = reinterpret_cast<glm::mat4x4*>(instanceMatricesBuf.data());

  // May be do more fast clean
  std::memset(nInstances.data(), 0, nInstances.size() * sizeof(nInstances[0]));
  for (std::size_t i = 0; i < instanceMatrices.size(); ++i)
  {
    const auto meshIdx = instanceMeshes[i];
    const auto& instanceMatrix = instanceMatrices[i];
    for (std::size_t j = 0; j < meshes[meshIdx].relemCount; j++)
    {
      const auto relemIdx = meshes[meshIdx].firstRelem + j;
      if (IsNotVisble(bbs[relemIdx], glob_tm * instanceMatrix))
      {
        continue;
      }
      nInstances[relemIdx]++;
      *matrices++ = instanceMatrix;
    }
  }
}
