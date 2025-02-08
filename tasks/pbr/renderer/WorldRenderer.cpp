#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include "stb_image.h"

#include <math.h>
#ifndef M_PIf
#define M_PIf 3.14159265358979323846f
#endif

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

  allocateGBuffer();
  loadSkybox();
}

void WorldRenderer::allocateGBuffer() {
  auto& albedo    = gBuffer[0];
  auto& normal    = gBuffer[1];
  auto& material  = gBuffer[2];
  auto& wc        = gBuffer[3];
  auto& depth     = gBuffer[4];

  auto& ctx = etna::get_context(); 

  albedo = ctx.createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer_albedo",
    .format = vk::Format::eB8G8R8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  normal = ctx.createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer_normal",
    .format = vk::Format::eR16G16B16A16Snorm,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  material = ctx.createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer_material",
    .format = vk::Format::eB8G8R8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  wc = ctx.createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer_wc",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  depth = ctx.createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
  });



  gBufferColorAttachments = {
    etna::RenderTargetState::AttachmentParams {
      .image = albedo.get(),
      .view  = albedo.getView({}),
      .imageAspect = vk::ImageAspectFlagBits::eColor,
    },
    etna::RenderTargetState::AttachmentParams {
      .image = normal.get(),
      .view  = normal.getView({}),
      .imageAspect = vk::ImageAspectFlagBits::eColor,
    },
    etna::RenderTargetState::AttachmentParams {
      .image = material.get(),
      .view  = material.getView({}),
      .imageAspect = vk::ImageAspectFlagBits::eColor,
    },
    etna::RenderTargetState::AttachmentParams {
      .image = wc.get(),
      .view  = wc.getView({}),
      .imageAspect = vk::ImageAspectFlagBits::eColor,
    },
  };

  gBufferDepthAttachment = etna::RenderTargetState::AttachmentParams {
    .image = depth.get(),
    .view  = depth.getView({}),
    .imageAspect = vk::ImageAspectFlagBits::eDepth,
  };
}

void WorldRenderer::loadSkybox() {
  auto& ctx = etna::get_context();


  skybox = ctx.createImage({
    .extent = vk::Extent3D{2048, 2048, 1},
    .name = "skybox",
    .format = vk::Format::eB8G8R8A8Srgb,
    .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
    .layers = 6,
    .flags = vk::ImageCreateFlagBits::eCubeCompatible,
  });
  
  for(size_t i = 0; i < 6; ++i) {
    int width, height, nChans;
    auto* imageBytes = stbi_load((GRAPHICS_COURSE_RESOURCES_ROOT "/textures/sb-" + std::to_string(i) + ".jpg").c_str(), &width, &height, &nChans, STBI_rgb_alpha);
    if (imageBytes == nullptr)
    {
      spdlog::log(spdlog::level::err, "Skybox load is unsuccessful");
      return;
    }
    size_t size = static_cast<std::size_t>(width * height * 4);
    etna::BlockingTransferHelper bth({.stagingSize = size});
    
    bth.uploadImage(*ctx.createOneShotCmdMgr(), skybox, 0, i, std::span<const std::byte>(reinterpret_cast<const std::byte*>(imageBytes), size));

  }
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
    {PBR_RENDERER_SHADERS_ROOT "static_mesh.frag.spv",
     PBR_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"}
  );
  
  etna::create_program(
    "perlin_shader",
    {PBR_RENDERER_SHADERS_ROOT "quad.vert.spv",
     PBR_RENDERER_SHADERS_ROOT "perlin.frag.spv"}
  );

  etna::create_program(
    "terrain_shader",
    {PBR_RENDERER_SHADERS_ROOT "terrain.vert.spv",
     PBR_RENDERER_SHADERS_ROOT "terrain.tesc.spv",
     PBR_RENDERER_SHADERS_ROOT "terrain.tese.spv",
     PBR_RENDERER_SHADERS_ROOT "terrain.frag.spv"
     }
  );

  etna::create_program(
    "distribution_compute",
    {PBR_RENDERER_SHADERS_ROOT "distribution.comp.spv"}
  );

    etna::create_program(
    "histogram_compute",
    {PBR_RENDERER_SHADERS_ROOT "histogram.comp.spv"}
  );

  etna::create_program(
    "postprocess_shader",
    {PBR_RENDERER_SHADERS_ROOT "quad.vert.spv",
     PBR_RENDERER_SHADERS_ROOT "postprocess.frag.spv"}
  );

  etna::create_program(
    "deferred_shader",
    {PBR_RENDERER_SHADERS_ROOT "deferred.vert.spv",
     PBR_RENDERER_SHADERS_ROOT "deferred.frag.spv"}
  );

  etna::create_program(
    "skybox_shader",
    {PBR_RENDERER_SHADERS_ROOT "skybox.vert.spv",
     PBR_RENDERER_SHADERS_ROOT "skybox.frag.spv"}
  );

  etna::create_program(
    "sphere_deferred_shader",
    {PBR_RENDERER_SHADERS_ROOT "sphere.vert.spv",
     PBR_RENDERER_SHADERS_ROOT "sphere_deferred.frag.spv"}
  );

  etna::create_program(
    "sphere_shader",
    {PBR_RENDERER_SHADERS_ROOT "sphere.vert.spv",
     PBR_RENDERER_SHADERS_ROOT "sphere.frag.spv"}
  );
  etna::create_program("static_mesh", {PBR_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
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
      .blendingConfig = {
        .attachments = {
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
        },
        .logicOpEnable = false,
        .logicOp = {},
        .blendConstants = {},
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {
            gBuffer[0].getFormat(), /*albedo*/
            gBuffer[1].getFormat(), /*normal*/
            gBuffer[2].getFormat(), /*materials*/
            gBuffer[3].getFormat(), /*wc*/
          },
          .depthAttachmentFormat = gBuffer.back().getFormat(),
        },
    });

   terrainPipeline = pipelineManager.createGraphicsPipeline(
    "terrain_shader",
    etna::GraphicsPipeline::CreateInfo{
      .inputAssemblyConfig = {.topology = vk::PrimitiveTopology::ePatchList},
      .tessellationConfig = {
        .patchControlPoints = 4,
      },
      
      .blendingConfig = {
        .attachments = {
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
        },
        .logicOpEnable = false,
        .logicOp = {},
        .blendConstants = {},
      },

      .fragmentShaderOutput =
      {
        .colorAttachmentFormats = {
          gBuffer[0].getFormat(), /*albedo*/
          gBuffer[1].getFormat(), /*normal*/
          gBuffer[2].getFormat(), /*materials*/
          gBuffer[3].getFormat(), /*wc*/
        },
        .depthAttachmentFormat = gBuffer.back().getFormat(),
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
      .blendingConfig = {
        .attachments = {
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
        },
        .logicOpEnable = false,
        .logicOp = {},
        .blendConstants = {},
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {
            gBuffer[0].getFormat(), /*albedo*/
            gBuffer[1].getFormat(), /*normal*/
            gBuffer[2].getFormat(), /*materials*/
            gBuffer[3].getFormat(), /*wc*/
          },
          .depthAttachmentFormat = gBuffer.back().getFormat(),
        },
    });

    skyboxPipeline = pipelineManager.createGraphicsPipeline(
    "skybox_shader",
    etna::GraphicsPipeline::CreateInfo{
      .depthConfig = {
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .maxDepthBounds = 1.f,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {BACKBUFFER_FORMAT},
          .depthAttachmentFormat = gBuffer.back().getFormat(),
        },
    });

    deferredLightPipeline = pipelineManager.createGraphicsPipeline(
    "deferred_shader",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {BACKBUFFER_FORMAT},
        },
    });

    spherePipeline = pipelineManager.createGraphicsPipeline(
    "sphere_shader",
    etna::GraphicsPipeline::CreateInfo{
      .inputAssemblyConfig = { 
        .topology = vk::PrimitiveTopology::eTriangleStrip,
      },
      .rasterizationConfig = {
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .lineWidth = 1.f,
      },
     
      .blendingConfig = {
        .attachments = {
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::True,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOne,
            .colorBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
        },
        .logicOpEnable = false,
        .logicOp = {},
        .blendConstants = {},
      },
      .depthConfig = {
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::False,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .maxDepthBounds = 1.f,
      },
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {BACKBUFFER_FORMAT},
        .depthAttachmentFormat = gBuffer.back().getFormat(),
      },
    });

    sphereDeferredPipeline = pipelineManager.createGraphicsPipeline(
    "sphere_deferred_shader",
    etna::GraphicsPipeline::CreateInfo{
      .inputAssemblyConfig = { 
        .topology = vk::PrimitiveTopology::eTriangleStrip,
      },
      .rasterizationConfig = {
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eFront,
        .lineWidth = 1.f,
      },
     
      .blendingConfig = {
        .attachments = {
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::True,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOne,
            .colorBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
        },
        .logicOpEnable = false,
        .logicOp = {},
        .blendConstants = {},
      },
      .depthConfig = {
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::False,
        .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
        .maxDepthBounds = 1.f,
      },
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {BACKBUFFER_FORMAT},
        .depthAttachmentFormat = gBuffer.back().getFormat(),
      },
    });

  histogramPipeline = pipelineManager.createComputePipeline("histogram_compute", {});
  distributionPipeline = pipelineManager.createComputePipeline("distribution_compute", {});
  
  postprocessPipeline = pipelineManager.createGraphicsPipeline(
    "postprocess_shader",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {swapchain_format},
        },
    });


}

void WorldRenderer::debugInput(const Keyboard& kb) 
{
  if (kb[KeyboardKey::kF3] == ButtonState::Falling) {
    wireframe = !wireframe;
    spdlog::info("terrain wireframe is {}", wireframe ? "on" : "off");
  }
  if (kb[KeyboardKey::kN] == ButtonState::Falling) {
    normalMap = !normalMap;
    spdlog::info("normal maps are {}", normalMap ? "on" : "off");
  }
  if (kb[KeyboardKey::kPause] == ButtonState::Falling)
  {
    pause = !pause;
    spdlog::info("Pause is {}", useToneMap ? "on" : "off");
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
    worldView = packet.mainCam.viewTm();
    worldProj = packet.mainCam.projTm(aspect);
    pushConstantsTerrain.camPos = packet.mainCam.position;
  }
  if(!pause) {
    frameTime = packet.currentTime; 
  }
  
}

void WorldRenderer::renderScene(
  vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout)
{
  if (!sceneMgr->getVertexBuffer())
    return;

  prepareFrame(glob_tm);

  auto staticMesh = etna::get_shader_program("static_mesh_material");

  cmd_buf.bindVertexBuffers(0, {sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);


  pushConst2M.projView = worldViewProj;
  auto set0 = etna::create_descriptor_set(
  staticMesh.getDescriptorLayoutId(0),
  cmd_buf,
  {
    etna::Binding{0, instanceMatricesBuf.genBinding()},
  });
  auto relems = sceneMgr->getRenderElements();
  std::size_t firstInstance = 0;
  for (std::size_t j = 0; j < relems.size(); ++j)
  {
    //Skip drawing water as it is rendered by terrain
    if (j == 8) {
      firstInstance += nInstances[j];
      continue;
    }  
    if (nInstances[j] != 0)
    {
      Material::Id mid = relems[j].materialId;
      auto& material = sceneMgr->get(mid == Material::Id::Invalid ? sceneMgr->getStubMaterial() : relems[j].materialId);
      auto& baseColorImage = sceneMgr->get(material.baseColorTexture).image;
      auto& normalImage = normalMap ? sceneMgr->get(material.normalTexture).image : sceneMgr->get(sceneMgr->getStubBlueTexture()).image;
      auto& metallicRoughnessImage = normalMap ? sceneMgr->get(material.metallicRoughnessTexture).image : sceneMgr->get(sceneMgr->getStubTexture()).image;
      auto set1 = etna::create_descriptor_set(
        staticMesh.getDescriptorLayoutId(1),
        cmd_buf,
        {
          etna::Binding{0, baseColorImage        .genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding{1, normalImage           .genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding{2, metallicRoughnessImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        });
      pushConst2M.color = material.baseColor;
      pushConst2M.emr_factors = material.EMR_Factor;
      cmd_buf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        staticMeshPipeline.getVkPipelineLayout(),
        0,
        {set0.getVkSet(), set1.getVkSet()},
        {});

      const auto& relem = relems[j];
      cmd_buf.pushConstants<PushConstants>(
        pipeline_layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, {pushConst2M});
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
      vk::AccessFlagBits2::eTransferWrite, 
      vk::ImageLayout::eTransferDstOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
    etna::set_state(cmd_buf, 
      backbuffer.get(), 
      vk::PipelineStageFlagBits2::eTransfer, 
      vk::AccessFlagBits2::eTransferRead, 
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

  for(auto& atts: gBufferColorAttachments) atts.loadOp = vk::AttachmentLoadOp::eClear;
  gBufferDepthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  
  etna::RenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {resolution.x, resolution.y}},
    gBufferColorAttachments,
    gBufferDepthAttachment
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

  pushConstantsTerrain.extent = {step, step};
  pushConstantsTerrain.mat  = worldViewProj;
  pushConstantsTerrain.degree = 256;
  for (size_t x = 0; x < nChunks; ++x) {
    for (size_t y = 0; y < nChunks; ++y) {
      pushConstantsTerrain.base = {-1.f + x * step, -1.f + y * step};

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

  for(std::size_t i = 0; i < gBuffer.size() - 1; i++) {
    etna::set_state(cmd_buf, 
      gBuffer[i].get(), 
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
      vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
      vk::ImageLayout::eColorAttachmentOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
  }
  etna::set_state(cmd_buf, 
      gBuffer.back().get(), 
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
      vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
      vk::ImageLayout::eDepthAttachmentOptimal, 
      vk::ImageAspectFlagBits::eDepth
    );
  etna::flush_barriers(cmd_buf);

  renderTerrain(cmd_buf);

  {
    ETNA_PROFILE_GPU(cmd_buf, renderForward);

    for(auto& atts: gBufferColorAttachments) atts.loadOp = vk::AttachmentLoadOp::eLoad;
    gBufferDepthAttachment.loadOp = vk::AttachmentLoadOp::eLoad;

    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      gBufferColorAttachments,
      gBufferDepthAttachment
    );
    

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, staticMeshPipeline.getVkPipeline());
    renderScene(cmd_buf, worldViewProj, staticMeshPipeline.getVkPipelineLayout());
  }

  renderLights(cmd_buf);
  renderPostprocess(cmd_buf, target_image, target_image_view);
}

static bool IsNotVisble(const glm::mat2x3 bounds, const glm::mat4x4& transform)
{
  return false;
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



void WorldRenderer::renderLights(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, renderLights);

  etna::set_state(cmd_buf, 
    backbuffer.get(), 
    vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
    {}, 
    vk::ImageLayout::eColorAttachmentOptimal, 
    vk::ImageAspectFlagBits::eColor
  );

   for(std::size_t i = 0; i < gBuffer.size() - 1; i++) {
    etna::set_state(cmd_buf, 
      gBuffer[i].get(), 
      vk::PipelineStageFlagBits2::eFragmentShader, 
      vk::AccessFlagBits2::eShaderSampledRead, 
      vk::ImageLayout::eShaderReadOnlyOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
  }
  etna::set_state(cmd_buf, 
      gBuffer.back().get(), 
      vk::PipelineStageFlagBits2::eFragmentShader, 
      vk::AccessFlagBits2::eShaderSampledRead, 
      vk::ImageLayout::eShaderReadOnlyOptimal, 
      vk::ImageAspectFlagBits::eDepth
    );
  etna::flush_barriers(cmd_buf);
  {
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = backbuffer.get(), .view = backbuffer.getView({})}},
      {}
    );

    auto deferredLightShader = etna::get_shader_program("deferred_shader");
    auto& pipeline = deferredLightPipeline;

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

    auto set = etna::create_descriptor_set(
      deferredLightShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, gBuffer[0].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{1, gBuffer[1].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{2, gBuffer[2].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{3, gBuffer[3].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{4, gBuffer[4].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{5, skybox.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {.layerCount=6, .type=vk::ImageViewType::eCube})}
      }
    );
    assert(gBuffer.size() == 5);
    
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      pipeline.getVkPipelineLayout(),
      0,
      {set.getVkSet()},
      {}
    );

    LightSource::Id sunId = static_cast<LightSource::Id>(0);
    struct {glm::mat4x4 p, v; glm::vec4 pos, color;} pushConstants{worldProj, worldView, sceneMgr->getLights()[sunId].position, sceneMgr->getLights()[sunId].colorRange};

    cmd_buf.pushConstants(
      pipeline.getVkPipelineLayout(), 
      vk::ShaderStageFlagBits::eFragment,
      0,
      uint32_t(sizeof(pushConstants)),
      &pushConstants
    );

    cmd_buf.draw(3, 1, 0, 0);
  }
  renderSphereDeferred(cmd_buf);
  renderSkybox(cmd_buf);
  renderSphere(cmd_buf);
}

namespace {
class CustomRenderTargetState {
  vk::CommandBuffer commandBuffer;
  static bool inScope;

public:
  struct AttachmentParams
  {
    vk::Image image = VK_NULL_HANDLE;
    vk::ImageView view = VK_NULL_HANDLE;
    std::optional<vk::ImageAspectFlags> imageAspect{};
    vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    vk::ClearColorValue clearColorValue = std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f});
    vk::ClearDepthStencilValue clearDepthStencilValue = {1.0f, 0};
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;

    // By default, the render target can work with multisample images and pipelines,
    // but not produce a final single-sample result.
    // These fields below are for the final MSAA image.
    // Ignore unless you know what MSAA is and aren't sure you need it.
    vk::Image resolveImage = VK_NULL_HANDLE;
    vk::ImageView resolveImageView = VK_NULL_HANDLE;
    std::optional<vk::ImageAspectFlags> resolveImageAspect{};
    vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone;
  };

  CustomRenderTargetState(
    vk::CommandBuffer cmd_buff,
    vk::Rect2D rect,
    const std::vector<AttachmentParams>& color_attachments,
    AttachmentParams depth_attachment,
    AttachmentParams stencil_attachment);
  ~CustomRenderTargetState();
  };
bool CustomRenderTargetState::inScope = false;

CustomRenderTargetState::CustomRenderTargetState(
  vk::CommandBuffer cmd_buff,
  vk::Rect2D rect,
  const std::vector<AttachmentParams>& color_attachments,
  AttachmentParams depth_attachment,
  AttachmentParams stencil_attachment)
{
  ETNA_VERIFYF(!inScope, "RenderTargetState scopes shouldn't overlap.");
  inScope = true;
  commandBuffer = cmd_buff;
  vk::Viewport viewport{
    .x = static_cast<float>(rect.offset.x),
    .y = static_cast<float>(rect.offset.y),
    .width = static_cast<float>(rect.extent.width),
    .height = static_cast<float>(rect.extent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  commandBuffer.setViewport(0, {viewport});
  commandBuffer.setScissor(0, {rect});

  std::vector<vk::RenderingAttachmentInfo> attachmentInfos(color_attachments.size());
  for (uint32_t i = 0; i < color_attachments.size(); ++i)
  {
    attachmentInfos[i].imageView = color_attachments[i].view;
    attachmentInfos[i].imageLayout = (color_attachments[i].layout != vk::ImageLayout::eUndefined) ?  color_attachments[i].layout : vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfos[i].loadOp = color_attachments[i].loadOp;
    attachmentInfos[i].storeOp = color_attachments[i].storeOp;
    attachmentInfos[i].clearValue = color_attachments[i].clearColorValue;
  }

  vk::RenderingAttachmentInfo depthAttInfo{
    .imageView = depth_attachment.view,
    .imageLayout = (depth_attachment.layout != vk::ImageLayout::eUndefined) ? depth_attachment.layout : vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .resolveMode = depth_attachment.resolveMode,
    .resolveImageView = depth_attachment.resolveImageView,
    .resolveImageLayout = vk::ImageLayout::eGeneral,
    .loadOp = depth_attachment.loadOp,
    .storeOp = depth_attachment.storeOp,
    .clearValue = depth_attachment.clearDepthStencilValue,
  };

  vk::RenderingAttachmentInfo stencilAttInfo{
    .imageView = stencil_attachment.view,
    .imageLayout = (stencil_attachment.layout != vk::ImageLayout::eUndefined) ? stencil_attachment.layout : vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .resolveMode = stencil_attachment.resolveMode,
    .resolveImageView = stencil_attachment.resolveImageView,
    .resolveImageLayout = vk::ImageLayout::eGeneral,
    .loadOp = stencil_attachment.loadOp,
    .storeOp = stencil_attachment.storeOp,
    .clearValue = stencil_attachment.clearDepthStencilValue,
  };

  vk::RenderingInfo renderInfo{
    .renderArea = rect,
    .layerCount = 1,
    .colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size()),
    .pColorAttachments = attachmentInfos.empty() ? nullptr : attachmentInfos.data(),
    .pDepthAttachment = depth_attachment.view ? &depthAttInfo : nullptr,
    .pStencilAttachment = stencil_attachment.view ? &stencilAttInfo : nullptr,
  };
  commandBuffer.beginRendering(renderInfo);
}

CustomRenderTargetState::~CustomRenderTargetState()
{
  commandBuffer.endRendering();
  inScope = false;
}

} /* namespace */

void WorldRenderer::renderSphereDeferred(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, renderSphereDeferred);
  etna::set_state(cmd_buf, 
    gBuffer.back().get(), 
    vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, 
    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eDepthStencilAttachmentRead, 
    vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal, 
    vk::ImageAspectFlagBits::eDepth
  );
  etna::flush_barriers(cmd_buf);
  CustomRenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {resolution.x, resolution.y}},
    {{.image = backbuffer.get(), .view = backbuffer.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad,}},
    {.image = gBuffer.back().get(), .view = gBuffer.back().getView({}), .loadOp = vk::AttachmentLoadOp::eLoad, .storeOp = vk::AttachmentStoreOp::eNone, .layout=vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal},
    {}
  );
  auto sphereShader = etna::get_shader_program("sphere_deferred_shader");
  auto& pipeline = sphereDeferredPipeline;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

  auto set = etna::create_descriptor_set(
      sphereShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, gBuffer[0].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{1, gBuffer[1].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{2, gBuffer[2].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{3, gBuffer[3].genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{4, gBuffer[4].genBinding(defaultSampler.get(), vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal)},
      }
    );
    assert(gBuffer.size() == 5);
    
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      pipeline.getVkPipelineLayout(),
      0,
      {set.getVkSet()},
      {}
    );
  auto& lights = sceneMgr->getLights();
  for(const auto& light : lights) {
    const float dist = glm::length(glm::vec3(worldViewProj * glm::vec4(light.position.x, light.position.y, light.position.z, 1)));
    const float fovCorrection = glm::length(glm::vec3(worldViewProj[0]));
    uint32_t n = static_cast<uint32_t>((5000.f * fovCorrection * light.colorRange.w / dist));
    if (n < 4) {
      continue;
    }
    n = std::min(n, 128u);
    struct {glm::mat4x4 pv, v; glm::vec4 pos, color; float degree;} pushConstants{worldProj, worldView, light.position, light.colorRange, M_PIf / n};
    pushConstants.pos += light.floatingAmplitude * glm::sin(light.floatingSpeed * static_cast<float>(frameTime));
    pushConstants.pos.w = light.position.w;
    cmd_buf.pushConstants(
      pipeline.getVkPipelineLayout(), 
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
      0,
      uint32_t(sizeof(pushConstants)),
      &pushConstants
    );

    cmd_buf.draw(2 * n + 2, n, 0, 0);
  }
}

void WorldRenderer::renderSphere(vk::CommandBuffer cmd_buf)
{
  ETNA_PROFILE_GPU(cmd_buf, renderSphere);
  etna::set_state(cmd_buf, 
    gBuffer.back().get(), 
    vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, 
    vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eDepthStencilAttachmentRead, 
    vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal, 
    vk::ImageAspectFlagBits::eDepth
  );
  etna::flush_barriers(cmd_buf);
  CustomRenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {resolution.x, resolution.y}},
    {{.image = backbuffer.get(), .view = backbuffer.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad,}},
    {.image = gBuffer.back().get(), .view = gBuffer.back().getView({}), .loadOp = vk::AttachmentLoadOp::eLoad, .storeOp = vk::AttachmentStoreOp::eNone, .layout=vk::ImageLayout::eDepthReadOnlyStencilAttachmentOptimal},
    {}
  );
  auto& pipeline = spherePipeline;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

  auto& lights = sceneMgr->getLights();
  for(const auto& light : lights) {
    const float dist = glm::length(glm::vec3(worldViewProj * glm::vec4(light.position.x, light.position.y, light.position.z, 1)));
    const float fovCorrection = glm::length(glm::vec3(worldViewProj[0]));
    uint32_t n = static_cast<uint32_t>((900.f * fovCorrection * light.visibleRadius / dist));
    if (n == 0) continue;
    n = std::min(n, 128u);
    n = std::max(n,   5u);
    struct {glm::mat4x4 pv, v; glm::vec4 pos, color; float degree;} pushConstants{worldProj, worldView, light.position, light.colorRange, M_PIf / n};
    pushConstants.pos.w = light.visibleRadius;
    pushConstants.pos += light.floatingAmplitude * glm::sin(light.floatingSpeed * static_cast<float>(frameTime));
    cmd_buf.pushConstants(
      pipeline.getVkPipelineLayout(), 
      vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
      0,
      uint32_t(sizeof(pushConstants)),
      &pushConstants
    );

    cmd_buf.draw(2 * n + 2, n, 0, 0);
  }
}

void WorldRenderer::renderSkybox(vk::CommandBuffer cmd_buf) {
  ETNA_PROFILE_GPU(cmd_buf, renderSkybox);
  {
    etna::set_state(cmd_buf, 
      gBuffer.back().get(), 
      vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, 
      vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite, 
      vk::ImageLayout::eDepthStencilAttachmentOptimal, 
      vk::ImageAspectFlagBits::eDepth
    );
    etna::flush_barriers(cmd_buf);
    CustomRenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = backbuffer.get(), .view = backbuffer.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad,}},
      {.image = gBuffer.back().get(), .view = gBuffer.back().getView({}), .loadOp = vk::AttachmentLoadOp::eLoad, .storeOp = vk::AttachmentStoreOp::eStore, .layout=vk::ImageLayout::eDepthStencilAttachmentOptimal},
      {}
    );

    auto skyboxShader = etna::get_shader_program("skybox_shader");
    auto& pipeline = skyboxPipeline;

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

    auto set = etna::create_descriptor_set(
      skyboxShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, skybox.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {.layerCount=6, .type=vk::ImageViewType::eCube})}
      }
    );
    
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      pipeline.getVkPipelineLayout(),
      0,
      {set.getVkSet()},
      {}
    );

    cmd_buf.pushConstants(
      pipeline.getVkPipelineLayout(), 
      vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex,
      0,
      uint32_t(sizeof(worldViewProj)),
      &worldViewProj
    );

    cmd_buf.draw(3, 1, 0, 0);
  }
}