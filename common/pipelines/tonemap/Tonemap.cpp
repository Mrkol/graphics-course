#include "Tonemap.hpp"

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
TonemapPipeline::allocate()
{
    auto& ctx = etna::get_context();
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
    
    defaultSampler = etna::Sampler({
      .filter = vk::Filter::eLinear,
      .name = "tonemapSampler",
    });
}

void 
TonemapPipeline::loadShaders() 
{
    etna::create_program(
        "distribution_compute",
        {TONEMAP_PIPELINE_SHADERS_ROOT "distribution.comp.spv"}
    );

    etna::create_program(
        "histogram_compute",
        {TONEMAP_PIPELINE_SHADERS_ROOT "histogram.comp.spv"}
    );
    etna::create_program(
        "tonemap_shader",
        { TONEMAP_PIPELINE_SHADERS_ROOT "tonemap.vert.spv",
          TONEMAP_PIPELINE_SHADERS_ROOT "tonemap.frag.spv"}
    );
}

void 
TonemapPipeline::setup() 
{
    auto& pipelineManager = etna::get_context().getPipelineManager();

    pipeline = pipelineManager.createGraphicsPipeline(
    "tonemap_shader",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
          .depthAttachmentFormat  = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
        },
    });

    histogramPipeline = pipelineManager.createComputePipeline("histogram_compute", {});

    distributionPipeline = pipelineManager.createComputePipeline("distribution_compute", {});
  
}

void 
TonemapPipeline::drawGui()
{
  ImGui::Checkbox("Enable tonemap [WIP]", &enable);
}

void 
TonemapPipeline::debugInput(const Keyboard& kb)
{
  if (kb[KeyboardKey::kT] == ButtonState::Falling)
  {
    enable = !enable;
    spdlog::info("Tonemap is {}", enable ? "on" : "off");
  }
}

void 
TonemapPipeline::render(vk::CommandBuffer cmd_buf, targets::Backbuffer& source, const RenderContext& /*ctx*/)
{
  ETNA_PROFILE_GPU(cmd_buf, renderPostprocess);

  auto postprocessShader = etna::get_shader_program("tonemap_shader");
  
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

  auto set = etna::create_descriptor_set(
    postprocessShader.getDescriptorLayoutId(0),
    cmd_buf,
    {
      etna::Binding{0, source.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
      etna::Binding{1, distributionBuffer.genBinding()}
    }
  );

  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eGraphics,
    pipeline.getVkPipelineLayout(),
    0,
    {set.getVkSet()},
    {}
  );

  cmd_buf.draw(3, 1, 0, 0);
}

void 
TonemapPipeline::tonemapEvaluate(vk::CommandBuffer cmd_buf, targets::Backbuffer& backbuffer, const RenderContext& ctx)
{
  ETNA_PROFILE_GPU(cmd_buf, tonemapEvaluate);
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

    cmd_buf.dispatch((ctx.resolution.x + 31) / 32, (ctx.resolution.y + 31) / 32, 1);
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

} /* namespace pipes */
