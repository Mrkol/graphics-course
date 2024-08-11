#include "simple_compute.h"

#include <fmt/ranges.h> // NOTE: vector and co are only printable with this included

#include <etna/Etna.hpp>
#include <etna/PipelineManager.hpp>

SimpleCompute::SimpleCompute()
  : length{16}
{
}

void SimpleCompute::setup()
{
  etna::create_program("simple_compute", {GRAPHICS_COURSE_ROOT"/resources/shaders/simple.comp.spv"});

  //// Buffer creation

  bufA = context->createBuffer(etna::Buffer::CreateInfo
    {
      .size = sizeof(float) * length,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .name = "A"
    }
  );

  bufB = context->createBuffer(etna::Buffer::CreateInfo
    {
      .size = sizeof(float) * length,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .name = "B"
    }
  );

  bufResult = context->createBuffer(etna::Buffer::CreateInfo
    {
      .size = sizeof(float) * length,
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
      .name = "m_sum"
    }
  );

  //// Filling the buffers

  {
    std::vector<float> values(length);
    for (uint32_t i = 0; i < values.size(); ++i) {
      values[i] = (float)i;
    }
    transferHelper->uploadBuffer<float>(*cmdMgr, bufA, 0, values);
  }

  {
    std::vector<float> values(length);
    for (uint32_t i = 0; i < values.size(); ++i) {
      values[i] = (float)i * i;
    }
    transferHelper->uploadBuffer<float>(*cmdMgr, bufB, 0, values);
  }
  //// Compute pipeline creation
  pipeline = context->getPipelineManager().createComputePipeline("simple_compute", {});
}

void SimpleCompute::buildCommandBuffer(vk::CommandBuffer cmd_buf)
{
  ETNA_CHECK_VK_RESULT(cmd_buf.begin(vk::CommandBufferBeginInfo{}));

  auto simpleComputeInfo = etna::get_shader_program("simple_compute");

  auto set = etna::create_descriptor_set(simpleComputeInfo.getDescriptorLayoutId(0), cmd_buf,
    {
      etna::Binding {0, bufA.genBinding()},
      etna::Binding {1, bufB.genBinding()},
      etna::Binding {2, bufResult.genBinding()},
    }
  );

  vk::DescriptorSet vkSet = set.getVkSet();

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
  cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, NULL);

  cmd_buf.pushConstants(pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(length), &length);

  etna::flush_barriers(cmd_buf);

  cmd_buf.dispatch(1, 1, 1);

  ETNA_CHECK_VK_RESULT(cmd_buf.end());
}

void SimpleCompute::readback()
{
  std::vector<float> values(length);
  transferHelper->readbackBuffer<float>(*cmdMgr, values, bufResult, 0);

  spdlog::info("Result on cpu:\n{}", values);
}
