#include "Perlin.hpp"

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
PerlinPipeline::allocate()
{
    // auto& ctx = etna::get_context();
    m_extent = {4096, 4096};
    auto& ctx = etna::get_context();
    m_images[0] = ctx.createImage({
        .extent = {m_extent.width, m_extent.height, 1},
        .name = "perlin0",
        .format = vk::Format::eR32Sfloat,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });

    m_images[1] = ctx.createImage({
        .extent = {m_extent.width, m_extent.height, 1},
        .name = "perlin1",
        .format = vk::Format::eR32Sfloat,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });
    m_sampler = etna::Sampler({
        .filter = vk::Filter::eLinear,
        .name = "perlinSample",
    });

    pipeline = ctx.getPipelineManager().createGraphicsPipeline("perlin_shader", {
        .fragmentShaderOutput = {
            .colorAttachmentFormats = {vk::Format::eR32Sfloat}
        },
    });
}

void 
PerlinPipeline::loadShaders() 
{
    etna::create_program(
        "perlin_shader",
        { PERLIN_PIPELINE_SHADERS_ROOT "perlin.vert.spv",
          PERLIN_PIPELINE_SHADERS_ROOT "perlin.frag.spv"}
    );
}

void 
PerlinPipeline::setup() 
{
    auto& pipelineManager = etna::get_context().getPipelineManager();

    pipeline = pipelineManager.createGraphicsPipeline(
    "perlin_shader",
    etna::GraphicsPipeline::CreateInfo{
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {vk::Format::eR32Sfloat},
          .depthAttachmentFormat  = vk::Format::eUndefined,
        },
    });
    render(*etna::get_context().createOneShotCmdMgr());
}

void 
PerlinPipeline::drawGui()
{
    if(ImGui::TreeNode("perlin")) {
        ImGui::TreePop();
    }
}

void 
PerlinPipeline::debugInput(const Keyboard& /*kb*/)
{

}

void 
PerlinPipeline::upscale(vk::CommandBuffer cmd_buf)
{
  etna::Image&  inImage = m_images[    m_currentImage];
  etna::Image& outImage = m_images[1 - m_currentImage];

  ETNA_PROFILE_GPU(cmd_buf, pipelines_perlin_upscale);
  {
    auto shader = etna::get_shader_program("perlin_shader");
    auto set = etna::create_descriptor_set(shader.getDescriptorLayoutId(0),
        cmd_buf, {
            etna::Binding(0, inImage.genBinding(m_sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal))
        }
    );

    if (m_frequency == 1)
    {
        etna::set_state(cmd_buf, 
            inImage.get(), 
            vk::PipelineStageFlagBits2::eTopOfPipe, 
            {}, 
            vk::ImageLayout::eGeneral, 
            vk::ImageAspectFlagBits::eColor
        );
        etna::flush_barriers(cmd_buf);
    }

    etna::set_state(cmd_buf, 
        outImage.get(), 
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
        {}, 
        vk::ImageLayout::eColorAttachmentOptimal, 
        vk::ImageAspectFlagBits::eColor
    );

    etna::set_state(cmd_buf, 
        inImage.get(), 
        vk::PipelineStageFlagBits2::eFragmentShader, 
        vk::AccessFlagBits2::eShaderSampledRead, 
        vk::ImageLayout::eShaderReadOnlyOptimal, 
        vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd_buf);
  

    etna::RenderTargetState renderTargets(
        cmd_buf,
        {{0, 0}, m_extent},
        {{
            .image = outImage.get(), 
            .view = outImage.getView({}),
        }},
        {}
    );

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
    cmd_buf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipeline.getVkPipelineLayout(),
        0,
        {set.getVkSet()},
        {}
    );

    struct {float amplitude; unsigned frequency;} pParams{0.5f / m_frequency, m_frequency};
    cmd_buf.pushConstants(pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(pParams), &pParams);
    cmd_buf.draw(3, 1, 0, 0);
  }
  etna::set_state(cmd_buf, outImage.get(), vk::PipelineStageFlagBits2::eBottomOfPipe, {}, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor);
  etna::flush_barriers(cmd_buf);
}

void 
PerlinPipeline::render(etna::OneShotCmdMgr& cmd_mgr, int scale)
{
  reset();
  auto cmdBuf = cmd_mgr.start();

  ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
  {
    for(int i = 0; i < scale; i++)
    {
      upscale(cmdBuf);
      m_frequency *= 2;
      m_currentImage = 1 - m_currentImage;
    }
  }   
  ETNA_CHECK_VK_RESULT(cmdBuf.end());
  cmd_mgr.submitAndWait(std::move(cmdBuf));

  spdlog::info("Upscale to {} sucesfull", m_frequency);
}

} /* namespace pipes */
