#include "QuadRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/DescriptorSet.hpp>


QuadRenderer::QuadRenderer(CreateInfo info)
{
  rect = info.rect;

  programId = etna::get_program_id("quad_renderer");

  if (programId == etna::ShaderProgramId::Invalid)
    programId = etna::create_program(
      "quad_renderer",
      {RENDER_UTILS_SHADERS_ROOT "quad.vert.spv", RENDER_UTILS_SHADERS_ROOT "quad.frag.spv"});

  auto& pipelineManager = etna::get_context().getPipelineManager();
  pipeline = pipelineManager.createGraphicsPipeline(
    "quad_renderer",
    {
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {info.format},
        },
    });
}

void QuadRenderer::render(
  vk::CommandBuffer cmd_buf,
  vk::Image target_image,
  vk::ImageView target_image_view,
  const etna::Image& tex_to_draw,
  const etna::Sampler& sampler)
{
  auto programInfo = etna::get_shader_program(programId);
  auto set = etna::create_descriptor_set(
    programInfo.getDescriptorLayoutId(0),
    cmd_buf,
    {etna::Binding{
      0, tex_to_draw.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}});

  etna::RenderTargetState renderTargets(
    cmd_buf,
    rect,
    {{.image = target_image, .view = target_image_view, .loadOp = vk::AttachmentLoadOp::eLoad}},
    {});

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});

  cmd_buf.draw(3, 1, 0, 0);
}
