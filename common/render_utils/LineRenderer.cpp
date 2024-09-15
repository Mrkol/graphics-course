#include "LineRenderer.hpp"
#include "etna/BlockingTransferHelper.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/DescriptorSet.hpp>
#include <etna/OneShotCmdMgr.hpp>


LineRenderer::LineRenderer(CreateInfo info)
{
  programId = etna::get_program_id("line_renderer");

  if (programId == etna::ShaderProgramId::Invalid)
    programId = etna::create_program(
      "line_renderer",
      {RENDER_UTILS_SHADERS_ROOT "plain_color.vert.spv",
       RENDER_UTILS_SHADERS_ROOT "plain_color.frag.spv"});

  auto& pipelineManager = etna::get_context().getPipelineManager();
  pipeline = pipelineManager
               .createGraphicsPipeline(
                 "line_renderer",
                 {
                   .vertexShaderInput =
                     {
                       .bindings =
                         {
                           etna::VertexShaderInputDescription::Binding{
                             .byteStreamDescription =
                               etna::VertexByteStreamFormatDescription{
                                 .stride = 6 * sizeof(float),
                                 .attributes =
                                   {
                                     etna::VertexByteStreamFormatDescription::Attribute{
                                       .format = vk::Format::eR32G32B32Sfloat,
                                       .offset = 0,
                                     },
                                     etna::VertexByteStreamFormatDescription::Attribute{
                                       .format = vk::Format::eR32G32B32Sfloat,
                                       .offset = 3 * sizeof(float),
                                     },
                                   },
                               },
                           },
                         },
                     },
                   .inputAssemblyConfig =
                     {
                       .topology = vk::PrimitiveTopology::eLineList,
                     },
                   .rasterizationConfig =
                     {
                       .polygonMode = vk::PolygonMode::eFill,
                       .lineWidth = 1.f,
                     },
                   .fragmentShaderOutput =
                     {
                       .colorAttachmentFormats = {info.format},
                       .depthAttachmentFormat = vk::Format::eD32Sfloat,
                     },
                 });

  std::unique_ptr<etna::OneShotCmdMgr> oneShotCmdMgr = etna::get_context().createOneShotCmdMgr();

  etna::BlockingTransferHelper transferHelper(etna::BlockingTransferHelper::CreateInfo{
    .stagingSize =
      std::max(std::span(info.vertices).size_bytes(), std::span(info.indices).size_bytes()),
  });

  vertexBuffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
    .size = std::span(info.vertices).size_bytes(),
    .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
    .name = "lines_vertices",
  });
  indexBuffer = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
    .size = std::span(info.vertices).size_bytes(),
    .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
    .name = "lines_indices",
  });

  transferHelper.uploadBuffer<Vertex>(*oneShotCmdMgr, vertexBuffer, 0, info.vertices);
  transferHelper.uploadBuffer<std::uint32_t>(*oneShotCmdMgr, indexBuffer, 0, info.indices);

  indexCount = info.indices.size();
}

void LineRenderer::render(
  vk::CommandBuffer cmd_buf,
  vk::Rect2D rect,
  glm::mat4 proj_view,
  vk::Image target_image,
  vk::ImageView target_image_view,
  vk::Image depth_image,
  vk::ImageView depth_image_view)
{
  auto programInfo = etna::get_shader_program(programId);

  etna::RenderTargetState renderTargets(
    cmd_buf,
    rect,
    {{.image = target_image, .view = target_image_view, .loadOp = vk::AttachmentLoadOp::eLoad}},
    {.image = depth_image, .view = depth_image_view, .loadOp = vk::AttachmentLoadOp::eLoad});

  cmd_buf.bindVertexBuffers(0, {vertexBuffer.get()}, {0});
  cmd_buf.bindIndexBuffer(indexBuffer.get(), 0, vk::IndexType::eUint32);

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
  cmd_buf.pushConstants(
    programInfo.getPipelineLayout(),
    vk::ShaderStageFlagBits::eVertex,
    0,
    sizeof(glm::mat4),
    &proj_view);

  cmd_buf.drawIndexed(indexCount, 1, 0, 0, 0);
}
