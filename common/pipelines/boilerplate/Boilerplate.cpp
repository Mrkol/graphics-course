#include "Boilerplate.hpp"

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
BoilerplatePipeline::allocate()
{
    // auto& ctx = etna::get_context();
}

void 
BoilerplatePipeline::loadShaders() 
{
    etna::create_program(
        "boilerplate_shader",
        { BOILERPLATE_PIPELINE_SHADERS_ROOT "boilerplate.vert.spv",
          BOILERPLATE_PIPELINE_SHADERS_ROOT "boilerplate.frag.spv"}
    );
}

void 
BoilerplatePipeline::setup() 
{
    auto& pipelineManager = etna::get_context().getPipelineManager();

    pipeline = pipelineManager.createGraphicsPipeline(
    "boilerplate_shader",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
          .depthAttachmentFormat  = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
        },
    });
}

void 
BoilerplatePipeline::drawGui()
{
    if(ImGui::TreeNode("boilerplate")) {
        ImGui::TreePop();
    }
}

void 
BoilerplatePipeline::debugInput(const Keyboard& /*kb*/)
{

}

BoilerplatePipeline::RenderTarget& 
BoilerplatePipeline::render(vk::CommandBuffer cmd_buf, RenderTarget& target, const RenderContext& ctx)
{
  ETNA_PROFILE_GPU(cmd_buf, pipelines_boilerplate_render);
  {
    etna::RenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {target.getResolution().x, target.getResolution().y}},
      target.getColorAttachments(),
      target.getDepthAttachment(),
      {}
    );

    auto boilerplateShader = etna::get_shader_program("boilerplate_shader");

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

    auto set = etna::create_descriptor_set(
      boilerplateShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        // etna::Binding{0, texture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {.layerCount=6, .type=vk::ImageViewType::eCube})}
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
      uint32_t(sizeof(ctx.worldViewProj)),
      &ctx.worldViewProj
    );

    cmd_buf.draw(3, 1, 0, 0);
  }
    return target;
}

} /* namespace pipes */
