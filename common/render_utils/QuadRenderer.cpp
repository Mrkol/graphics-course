#include "QuadRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/DescriptorSet.hpp>


QuadRenderer::QuadRenderer(CreateInfo info)
{
  rect = info.rect;
  programId = etna::create_program(
    "quad_renderer",
    {GRAPHICS_COURSE_ROOT "/resources/shaders/quad3_vert.vert.spv",
     GRAPHICS_COURSE_ROOT "/resources/shaders/quad.frag.spv"});

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
