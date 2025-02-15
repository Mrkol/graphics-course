#include "ResolveGBuffer.hpp"

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
ResolveGBufferPipeline::allocate()
{
    // auto& ctx = etna::get_context();
}

void 
ResolveGBufferPipeline::loadShaders() 
{
  etna::create_program(
    "deferred_shader",
    {RESOLVEGBUFFER_PIPELINE_SHADERS_ROOT "deferred.vert.spv",
     RESOLVEGBUFFER_PIPELINE_SHADERS_ROOT "deferred.frag.spv"}
  );
  etna::create_program(
    "sphere_deferred_shader",
    {RESOLVEGBUFFER_PIPELINE_SHADERS_ROOT "sphere.vert.spv",
     RESOLVEGBUFFER_PIPELINE_SHADERS_ROOT "sphere_deferred.frag.spv"}
  );

  etna::create_program(
    "sphere_shader",
    {RESOLVEGBUFFER_PIPELINE_SHADERS_ROOT "sphere.vert.spv",
     RESOLVEGBUFFER_PIPELINE_SHADERS_ROOT "sphere.frag.spv"}
  );

  defaultSampler = etna::Sampler({
    .filter = vk::Filter::eLinear,
    .name = "resolveGbufferSampler",
  });
}

void 
ResolveGBufferPipeline::setup() 
{
  auto& pipelineManager = etna::get_context().getPipelineManager();

  deferredLightPipeline = pipelineManager.createGraphicsPipeline(
  "deferred_shader",
  etna::GraphicsPipeline::CreateInfo{
    .depthConfig = {
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLess,
      .maxDepthBounds = 1.f,
    },
    .fragmentShaderOutput = {
      .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
      .depthAttachmentFormat = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
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
      .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
      .depthAttachmentFormat = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
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
      .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
      .depthAttachmentFormat = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
    },
  });
}

void 
ResolveGBufferPipeline::drawGui()
{
    if(ImGui::TreeNode("resolvegbuffer")) {
        ImGui::TreePop();
    }
}

void 
ResolveGBufferPipeline::debugInput(const Keyboard& /*kb*/)
{

}

void
ResolveGBufferPipeline::render(vk::CommandBuffer cmd_buf, targets::GBuffer& source, const RenderContext& ctx, const etna::Image& skybox)
{
  ETNA_PROFILE_GPU(cmd_buf, pipelines_resolvegbuffer_render);
  {
    auto deferredLightShader = etna::get_shader_program("deferred_shader");
    auto& pipeline = deferredLightPipeline;

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

    auto set = etna::create_descriptor_set(
      deferredLightShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, source.getImage(0).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{1, source.getImage(1).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{2, source.getImage(2).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{3, source.getImage(3).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{4, source.getImage(4).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{5, skybox.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {.layerCount=6, .type=vk::ImageViewType::eCube})}
      }
    );
    
    cmd_buf.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      pipeline.getVkPipelineLayout(),
      0,
      {set.getVkSet()},
      {}
    );

    LightSource::Id sunId = static_cast<LightSource::Id>(0);
    struct {glm::mat4x4 p, v; glm::vec4 pos, color;} pushConstants{ctx.worldProj, ctx.worldView, ctx.sceneMgr->getLights()[sunId].position, ctx.sceneMgr->getLights()[sunId].colorRange};

    cmd_buf.pushConstants(
      pipeline.getVkPipelineLayout(), 
      vk::ShaderStageFlagBits::eFragment,
      0,
      uint32_t(sizeof(pushConstants)),
      &pushConstants
    );

    cmd_buf.draw(3, 1, 0, 0);
  }
  renderSphereDeferred(cmd_buf, source, ctx);
  renderSphere(cmd_buf, ctx);
}


void ResolveGBufferPipeline::renderSphereDeferred(vk::CommandBuffer cmd_buf, targets::GBuffer& source, const RenderContext& ctx)
{
  ETNA_PROFILE_GPU(cmd_buf, renderSphereDeferred);
  auto sphereShader = etna::get_shader_program("sphere_deferred_shader");
  auto& pipeline = sphereDeferredPipeline;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

  auto set = etna::create_descriptor_set(
      sphereShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, source.getImage(0).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{1, source.getImage(1).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{2, source.getImage(2).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{3, source.getImage(3).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        etna::Binding{4, source.getImage(4).genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      }
    );
    
  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    pipeline.getVkPipelineLayout(),
    0,
    {set.getVkSet()},
    {}
  );
  auto& lights = ctx.sceneMgr->getLights();
  for(const auto& light : lights) {
    const float dist = glm::length(glm::vec3(ctx.worldViewProj * glm::vec4(light.position.x, light.position.y, light.position.z, 1)));
    const float fovCorrection = glm::length(glm::vec3(ctx.worldViewProj[0]));
    uint32_t n = static_cast<uint32_t>((5000.f * fovCorrection * light.colorRange.w / dist));
    if (n < 4) {
      continue;
    }
    n = std::min(n, 128u);
    struct {glm::mat4x4 pv, v; glm::vec4 pos, color; float degree;} pushConstants{ctx.worldProj, ctx.worldView, light.position, light.colorRange, M_PIf / n};
    pushConstants.pos += light.floatingAmplitude * glm::sin(light.floatingSpeed * static_cast<float>(ctx.frameTime));
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

void ResolveGBufferPipeline::renderSphere(vk::CommandBuffer cmd_buf, const RenderContext& ctx)
{
  auto& pipeline = spherePipeline;

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

  auto& lights = ctx.sceneMgr->getLights();
  for(const auto& light : lights) {
    const float dist = glm::length(glm::vec3(ctx.worldViewProj * glm::vec4(light.position.x, light.position.y, light.position.z, 1)));
    const float fovCorrection = glm::length(glm::vec3(ctx.worldViewProj[0]));
    uint32_t n = static_cast<uint32_t>((900.f * fovCorrection * light.visibleRadius / dist));
    if (n == 0) continue;
    n = std::min(n, 128u);
    n = std::max(n,   5u);
    struct {glm::mat4x4 pv, v; glm::vec4 pos, color; float degree;} pushConstants{ctx.worldProj, ctx.worldView, light.position, light.colorRange, M_PIf / n};
    pushConstants.pos.w = light.visibleRadius;
    pushConstants.pos += light.floatingAmplitude * glm::sin(light.floatingSpeed * static_cast<float>(ctx.frameTime));
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

} /* namespace pipes */
