#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>

const std::size_t N_MAX_INSTANCES = 1 << 14;
const vk::Format BACKBUFFER_FORMAT = vk::Format::eB10G11R11UfloatPack32;
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

  backbuffer = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "backbuffer",
    .format = BACKBUFFER_FORMAT,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage,
  });

    defaultSampler = etna::Sampler({
        .filter = vk::Filter::eLinear,
        .name = "perlinSample",
    });

  instanceMatricesBuf = ctx.createBuffer({
    .size = N_MAX_INSTANCES * sizeof(glm::mat4x4),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    .name = "instanceMatrices",
  });
  instanceMatricesBuf.map();

  histogramBuffer = ctx.createBuffer({
    .size = 128 * sizeof(int),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "histogram",
  });

  distributionBuffer = ctx.createBuffer({
    .size = 128 * sizeof(float),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    .name = "distribution",
  });

  heightmap.initImage({4096, 4096});
  for (std::size_t i = 0; i < 12; ++i)
  heightmap.upscale(*ctx.createOneShotCmdMgr());
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
    {TONEMAPPING_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     TONEMAPPING_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"}
  );
  
  etna::create_program(
    "perlin_shader",
    {TONEMAPPING_RENDERER_SHADERS_ROOT "quad.vert.spv",
     TONEMAPPING_RENDERER_SHADERS_ROOT "perlin.frag.spv"}
  );

  etna::create_program(
    "terrain_shader",
    {TONEMAPPING_RENDERER_SHADERS_ROOT "terrain.vert.spv",
     TONEMAPPING_RENDERER_SHADERS_ROOT "terrain.tesc.spv",
     TONEMAPPING_RENDERER_SHADERS_ROOT "terrain.tese.spv",
     TONEMAPPING_RENDERER_SHADERS_ROOT "terrain.frag.spv"
     }
  );

  etna::create_program(
    "distribution_compute",
    {TONEMAPPING_RENDERER_SHADERS_ROOT "distribution.comp.spv"}
  );

    etna::create_program(
    "histogram_compute",
    {TONEMAPPING_RENDERER_SHADERS_ROOT "histogram.comp.spv"}
  );

  etna::create_program(
    "postprocess_shader",
    {TONEMAPPING_RENDERER_SHADERS_ROOT "quad.vert.spv",
     TONEMAPPING_RENDERER_SHADERS_ROOT "postprocess.frag.spv"}
  );
  etna::create_program("static_mesh", {TONEMAPPING_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  spdlog::info("Shaders loaded");
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
          .colorAttachmentFormats = {BACKBUFFER_FORMAT},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });

   terrainPipeline = pipelineManager.createGraphicsPipeline(
    "terrain_shader",
    etna::GraphicsPipeline::CreateInfo{
      .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePatchList},
      .tessellationConfig = {
        .patchControlPoints = 4,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {BACKBUFFER_FORMAT},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });

   postprocessPipeline = pipelineManager.createGraphicsPipeline(
    "postprocess_shader",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {swapchain_format},
        },
    });
     terrainDebugPipeline = pipelineManager.createGraphicsPipeline(
    "terrain_shader",
    etna::GraphicsPipeline::CreateInfo{
      .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePatchList},
      .tessellationConfig = { 
        .patchControlPoints = 4,
      },
      .rasterizationConfig = {
        .polygonMode = vk::PolygonMode::eLine,
        .lineWidth = 1.f,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {BACKBUFFER_FORMAT},
          .depthAttachmentFormat = vk::Format::eD32Sfloat,
        },
    });

    histogramPipeline = pipelineManager.createComputePipeline("histogram_compute", {});
    distributionPipeline = pipelineManager.createComputePipeline("distribution_compute", {});
}

void WorldRenderer::debugInput(const Keyboard& kb) 
{
  if (kb[KeyboardKey::kF3] == ButtonState::Falling) {
    wireframe = !wireframe;
  }
  if (kb[KeyboardKey::kT] == ButtonState::Falling)
  {
    useToneMap = !useToneMap;
    spdlog::info("Tonemap is {}", useToneMap ? "on" : "off");
  }
}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
    pushConstantsTerrain.camPos = packet.mainCam.position;
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

void WorldRenderer::renderPostprocess(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderPostprocess);

  //For debug purposes only;
  if(!useToneMap)
  {
    etna::set_state(cmd_buf, 
      target_image, 
      vk::PipelineStageFlagBits2::eTransfer, 
      {}, 
      vk::ImageLayout::eTransferDstOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
    etna::set_state(cmd_buf, 
      backbuffer.get(), 
      vk::PipelineStageFlagBits2::eTransfer, 
      {}, 
      vk::ImageLayout::eTransferSrcOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd_buf);
    std::array<vk::Offset3D, 2> offs = {
        vk::Offset3D{},
        vk::Offset3D{.x =static_cast<int32_t>(resolution.x), .y=static_cast<int32_t>(resolution.y), .z = 1}
    };
    std::array<vk::ImageBlit, 1> blit = {
      vk::ImageBlit{
          .srcSubresource = {.aspectMask=vk::ImageAspectFlagBits::eColor, .layerCount=1,},
          .srcOffsets = offs,
          .dstSubresource = {.aspectMask=vk::ImageAspectFlagBits::eColor, .layerCount=1},
          .dstOffsets = offs,
        }
    };
    cmd_buf.blitImage(backbuffer.get(), vk::ImageLayout::eTransferSrcOptimal,
      target_image, vk::ImageLayout::eTransferDstOptimal,
      blit,
      vk::Filter::eLinear
    );
    etna::set_state(cmd_buf, 
      target_image, 
      vk::PipelineStageFlagBits2::eAllCommands, 
      {}, 
      vk::ImageLayout::ePresentSrcKHR, 
      vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd_buf);
    return;
  }
  tonemapEvaluate(cmd_buf);

  etna::set_state(cmd_buf, 
    backbuffer.get(), 
    vk::PipelineStageFlagBits2::eFragmentShader, 
    vk::AccessFlagBits2::eShaderSampledRead, 
    vk::ImageLayout::eShaderReadOnlyOptimal, 
    vk::ImageAspectFlagBits::eColor
  );
  etna::flush_barriers(cmd_buf);

  etna::RenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {resolution.x, resolution.y}},
    {{.image = target_image, .view = target_image_view}},
    {}
  );

  auto postprocessShader = etna::get_shader_program("postprocess_shader");
  
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, postprocessPipeline.getVkPipeline());

  auto set = etna::create_descriptor_set(
    postprocessShader.getDescriptorLayoutId(0),
    cmd_buf,
    {
      etna::Binding{0, backbuffer.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{1, distributionBuffer.genBinding()}
    }
  );

  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    postprocessPipeline.getVkPipelineLayout(),
    0,
    {set.getVkSet()},
    {}
  );

  cmd_buf.draw(3, 1, 0, 0);
}

void WorldRenderer::renderTerrain(
  vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, renderTerrain);
  auto& hmap = heightmap.getImage(); 

  etna::RenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {resolution.x, resolution.y}},
    {{.image = backbuffer.get(), .view = backbuffer.getView({})}},
    {.image = mainViewDepth.get(), .view = mainViewDepth.getView({})}
  );

  auto terrainShader = etna::get_shader_program("terrain_shader");
  auto& pipeline = wireframe ? terrainDebugPipeline : terrainPipeline;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

  auto set = etna::create_descriptor_set(
    terrainShader.getDescriptorLayoutId(0),
    cmd_buf,
    {etna::Binding{0, hmap.genBinding(heightmap.getSampler().get(), vk::ImageLayout::eShaderReadOnlyOptimal)}}
  );

  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    pipeline.getVkPipelineLayout(),
    0,
    {set.getVkSet()},
    {}
  );

  const size_t nChunks = 16;
  const float step = 2.f / nChunks;
  for (size_t x = 0; x < nChunks; ++x) {
    for (size_t y = 0; y < nChunks; ++y) {
      pushConstantsTerrain.base = {-1.f + x * step, -1.f + y * step};
      pushConstantsTerrain.extent = {step, step};
      pushConstantsTerrain.mat  = pushConst2M.projView;
      pushConstantsTerrain.degree = 1024;

        cmd_buf.pushConstants(
            pipeline.getVkPipelineLayout(), 
              vk::ShaderStageFlagBits::eVertex |
              vk::ShaderStageFlagBits::eTessellationEvaluation |
              vk::ShaderStageFlagBits::eTessellationControl,
              0, 
            sizeof(pushConstantsTerrain), &pushConstantsTerrain
        );

        cmd_buf.draw(4, 1, 0, 0);
    }
  }

}

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);

  etna::set_state(cmd_buf, 
    backbuffer.get(), 
    vk::PipelineStageFlagBits2::eFragmentShader, 
    {}, 
    vk::ImageLayout::eColorAttachmentOptimal, 
    vk::ImageAspectFlagBits::eColor
  );
  etna::flush_barriers(cmd_buf);

  renderTerrain(cmd_buf);

  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = backbuffer.get(), .view = backbuffer.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad}},
      {.image = mainViewDepth.get(), .view = mainViewDepth.getView({}), .loadOp=vk::AttachmentLoadOp::eLoad}
    );
    

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }

  renderPostprocess(cmd_buf, target_image, target_image_view);
}

static bool IsVisble(const glm::mat2x3 bounds, const glm::mat4x4& transform)
{
  glm::vec4 origin = glm::vec4(bounds[0], 1.f);
  glm::mat4x4 corners[2] = {
    {
      { bounds[1].x,  bounds[1].y,  bounds[1].z, 0.},
      { bounds[1].x,  bounds[1].y, -bounds[1].z, 0.},
      { bounds[1].x, -bounds[1].y,  bounds[1].z, 0.},
      { bounds[1].x, -bounds[1].y, -bounds[1].z, 0.},
    },
    {
      {-bounds[1].x,  bounds[1].y,  bounds[1].z, 0.},
      {-bounds[1].x,  bounds[1].y, -bounds[1].z, 0.},
      {-bounds[1].x, -bounds[1].y,  bounds[1].z, 0.},
      {-bounds[1].x, -bounds[1].y, -bounds[1].z, 0.},
    }};
  corners[0] += origin;
  corners[0] = transform * corners[0];

  for (size_t i = 0; i < 4; ++i)
    corners[0][i] /= corners[0][i][3];

  corners[1] += origin;
  corners[1] = transform * corners[1];

  for (size_t i = 0; i < 4; ++i)
    corners[1][i] /= corners[1][i][3];

  glm::vec3 min = corners[0][0];
  glm::vec3 max = corners[0][0];
  for (size_t i = 0; i < 2; ++i)
    for (size_t j = 0; j < 4; ++j)
    {
      min = glm::min(min, glm::vec3(corners[i][j]));
      max = glm::max(max, glm::vec3(corners[i][j]));
    }

  return min.x <  1. && 
         max.x > -1. && 
         min.y <  1. && 
         max.y > -1. && 
         min.y <  1. && 
         max.y > -1. ;
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
      if (!IsVisble(bbs[relemIdx], glob_tm * instanceMatrix))
      {
        continue;
      }
      nInstances[relemIdx]++;
      *matrices++ = instanceMatrix;
    }
  }
}

void WorldRenderer::tonemapEvaluate(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, tonemapEvaluate);
  etna::set_state(cmd_buf, 
    backbuffer.get(), 
    vk::PipelineStageFlagBits2::eComputeShader, 
    vk::AccessFlagBits2::eShaderRead, 
    vk::ImageLayout::eGeneral, 
    vk::ImageAspectFlagBits::eColor
  );
  etna::flush_barriers(cmd_buf);

  {
    std::array<vk::BufferMemoryBarrier2, 2> bmb = {
    vk::BufferMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
      .buffer = distributionBuffer.get(),
      .size = VK_WHOLE_SIZE,
     },
     vk::BufferMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
      .buffer = histogramBuffer.get(),
      .size = VK_WHOLE_SIZE,
     },
   };
    vk::DependencyInfo depInfo
    {
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
      .bufferMemoryBarrierCount=2,
      .pBufferMemoryBarriers = bmb.data(),
    };
    cmd_buf.pipelineBarrier2(depInfo);
  }

  //Compute histogramm
  {
    auto histogramShader = etna::get_shader_program("histogram_compute");
    
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, histogramPipeline.getVkPipeline());

    auto set = etna::create_descriptor_set(
      histogramShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, backbuffer.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)},
        etna::Binding{1, histogramBuffer.genBinding()}
      }
    );

    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      histogramPipeline.getVkPipelineLayout(),
      0,
      {set.getVkSet()},
      {}
    );

    cmd_buf.dispatch((resolution.x + 31) / 32, (resolution.y + 31) / 32, 1);
  }
  {  
  std::array<vk::BufferMemoryBarrier2, 2> bmb = {
    vk::BufferMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .buffer = distributionBuffer.get(),
      .size = VK_WHOLE_SIZE,
   },
   vk::BufferMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .buffer = histogramBuffer.get(),
      .size = VK_WHOLE_SIZE,
   }
  };
    vk::DependencyInfo depInfo
    {
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
      .bufferMemoryBarrierCount=2,
      .pBufferMemoryBarriers = bmb.data(),
    };
    cmd_buf.pipelineBarrier2(depInfo);
  }
  //Compute distribution
  {
    auto distributionShader = etna::get_shader_program("distribution_compute");
    
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, distributionPipeline.getVkPipeline());

    auto set = etna::create_descriptor_set(
      distributionShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, histogramBuffer.genBinding()},
        etna::Binding{1, distributionBuffer.genBinding()}
      }
    );
    
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eCompute,
      distributionPipeline.getVkPipelineLayout(),
      0,
      {set.getVkSet()},
      {}
    );

    cmd_buf.dispatch(1, 1, 1);
  }

  {
    std::array<vk::BufferMemoryBarrier2, 2> bmb = {
    vk::BufferMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
      .buffer = distributionBuffer.get(),
      .size = VK_WHOLE_SIZE,
     },
     vk::BufferMemoryBarrier2{
      .srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
      .dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
      .dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead,
      .buffer = histogramBuffer.get(),
      .size = VK_WHOLE_SIZE,
     },
   };
    vk::DependencyInfo depInfo
    {
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
      .bufferMemoryBarrierCount=2,
      .pBufferMemoryBarriers = bmb.data(),
    };
    cmd_buf.pipelineBarrier2(depInfo);
  }
}
