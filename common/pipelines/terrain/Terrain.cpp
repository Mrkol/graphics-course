#include "Terrain.hpp"

#include <etna/BlockingTransferHelper.hpp>
#include <etna/Etna.hpp>
#include <etna/Profiling.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>

#include <imgui.h>

#include "stb_image.h"

namespace pipes {

void 
TerrainPipeline::allocate()
{
    heightmapGenerator.allocate();
    // auto& ctx = etna::get_context();
}

void 
TerrainPipeline::loadShaders() 
{
    heightmapGenerator.loadShaders();
    etna::create_program(
        "terrain_shader",
        { TERRAIN_PIPELINE_SHADERS_ROOT "terrain.vert.spv",
          TERRAIN_PIPELINE_SHADERS_ROOT "terrain.tesc.spv",
          TERRAIN_PIPELINE_SHADERS_ROOT "terrain.tese.spv",
          TERRAIN_PIPELINE_SHADERS_ROOT "terrain.frag.spv"}
    );
}

void 
TerrainPipeline::setup() 
{
    heightmapGenerator.setup();
    auto& pipelineManager = etna::get_context().getPipelineManager();

    pipeline = pipelineManager.createGraphicsPipeline(
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
        .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
        .depthAttachmentFormat = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
      },
    });

    pipelineDebug = pipelineManager.createGraphicsPipeline(
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
          .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
          .depthAttachmentFormat = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
        },
    });
}

void 
TerrainPipeline::drawGui()
{
    if(ImGui::TreeNode("perlin settings")) {
        heightmapGenerator.drawGui();
        ImGui::TreePop();
    }
    ImGui::SliderInt("Terrain scale", &terrainScale, 1, 12);
    if(ImGui::Button("Regenerate")) {
        regenTerrain();
    }
    ImGui::Checkbox("Wireframe [F3]", &wireframe);
}

void 
TerrainPipeline::debugInput(const Keyboard& kb)
{
    heightmapGenerator.debugInput(kb);
    if (kb[KeyboardKey::kF3] == ButtonState::Falling) {
        wireframe = !wireframe;
        spdlog::info("terrain wireframe is {}", wireframe ? "on" : "off");
    }
}

targets::GBuffer& 
TerrainPipeline::render(vk::CommandBuffer cmd_buf, targets::GBuffer& target, const RenderContext& ctx)
{
  ETNA_PROFILE_GPU(cmd_buf, renderTerrain);
  auto& hmap = heightmapGenerator.getImage(); 
  
  #if 0
//FIXME
  for(auto& atts: gBufferColorAttachments) atts.loadOp = vk::AttachmentLoadOp::eClear;
  gBufferDepthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  
  etna::RenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {ctx.resolution.x, ctx.resolution.y}},
    target.getColorAttachments(),
    target.getDepthAttachment()
  );
  #endif

  auto terrainShader = etna::get_shader_program("terrain_shader");
  auto& currentPipeline = wireframe ? pipelineDebug : pipeline;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, currentPipeline.getVkPipeline());

  auto set = etna::create_descriptor_set(
    terrainShader.getDescriptorLayoutId(0),
    cmd_buf,
    {etna::Binding{0, hmap.genBinding(heightmapGenerator.getSampler().get(), vk::ImageLayout::eShaderReadOnlyOptimal)}}
  );

  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    currentPipeline.getVkPipelineLayout(),
    0,
    {set.getVkSet()},
    {}
  );

  const size_t nChunks = 16;
  const float step = 2.f / nChunks;

  pushConstants.extent = {step, step};
  pushConstants.mat  = ctx.worldViewProj;
  pushConstants.degree = 256;
  for (size_t x = 0; x < nChunks; ++x) {
    for (size_t y = 0; y < nChunks; ++y) {
      pushConstants.base = {-1.f + x * step, -1.f + y * step};

        cmd_buf.pushConstants(
            currentPipeline.getVkPipelineLayout(), 
              vk::ShaderStageFlagBits::eVertex |
              vk::ShaderStageFlagBits::eTessellationEvaluation |
              vk::ShaderStageFlagBits::eTessellationControl,
              0, 
            sizeof(pushConstants), &pushConstants
        );

        cmd_buf.draw(4, 1, 0, 0);
    }
  }

    return target;
}

void 
TerrainPipeline::regenTerrain()
{
  heightmapGenerator.render(*etna::get_context().createOneShotCmdMgr(), terrainScale);
  #if 0
  heightmap.reset();
  for (int i = 0; i < terrainScale; i++) {
    heightmap.upscale(*etna::get_context().createOneShotCmdMgr());
  }
  #endif
}

} /* namespace pipes */
