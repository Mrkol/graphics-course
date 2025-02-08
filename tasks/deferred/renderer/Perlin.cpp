#include "Perlin.hpp"
#include "etna/GlobalContext.hpp"
#include "etna/RenderTargetStates.hpp"



void PerlinGenerator::initImage(vk::Extent2D extent) 
{
    m_extent = extent;
    auto& ctx = etna::get_context();
    m_images[0] = ctx.createImage({
        .extent = {extent.width, extent.height, 1},
        .name = "perlin0",
        .format = vk::Format::eR32Sfloat,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });

    m_images[1] = ctx.createImage({
        .extent = {extent.width, extent.height, 1},
        .name = "perlin1",
        .format = vk::Format::eR32Sfloat,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });
    m_sampler = etna::Sampler({
        .filter = vk::Filter::eLinear,
        .name = "perlinSample",
    });

    m_pipeline = ctx.getPipelineManager().createGraphicsPipeline("perlin_shader", {
        .fragmentShaderOutput = {
            .colorAttachmentFormats = {vk::Format::eR32Sfloat}
        },
    });
}

void PerlinGenerator::upscale(etna::OneShotCmdMgr& cmd_mgr)
{
    auto cmdBuf = cmd_mgr.start();
    etna::Image&  inImage = m_images[    m_currentImage];
    etna::Image& outImage = m_images[1 - m_currentImage];

    ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {

        auto shader = etna::get_shader_program("perlin_shader");
        auto set = etna::create_descriptor_set(shader.getDescriptorLayoutId(0),
            cmdBuf, {
                etna::Binding(0, inImage.genBinding(m_sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal))
            }
        );

        if (m_frequency == 1)
        {
            etna::set_state(cmdBuf, 
                inImage.get(), 
                vk::PipelineStageFlagBits2::eTopOfPipe, 
                {}, 
                vk::ImageLayout::eGeneral, 
                vk::ImageAspectFlagBits::eColor
            );
            etna::flush_barriers(cmdBuf);
        }

        etna::set_state(cmdBuf, 
            outImage.get(), 
            vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
            {}, 
            vk::ImageLayout::eColorAttachmentOptimal, 
            vk::ImageAspectFlagBits::eColor
        );

        etna::set_state(cmdBuf, 
            inImage.get(), 
            vk::PipelineStageFlagBits2::eFragmentShader, 
            vk::AccessFlagBits2::eShaderSampledRead, 
            vk::ImageLayout::eShaderReadOnlyOptimal, 
            vk::ImageAspectFlagBits::eColor
        );
        etna::flush_barriers(cmdBuf);
      

        etna::RenderTargetState renderTargets(
            cmdBuf,
            {{0, 0}, m_extent},
            {{
                .image = outImage.get(), 
                .view = outImage.getView({}),
            }},
            {}
        );
    
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getVkPipeline());
        cmdBuf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            m_pipeline.getVkPipelineLayout(),
            0,
            {set.getVkSet()},
            {}
        );

        struct {float amplitude; unsigned frequency;} pParams{0.5f / m_frequency, m_frequency};
        cmdBuf.pushConstants(m_pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(pParams), &pParams);
        cmdBuf.draw(3, 1, 0, 0);
    }   
    etna::set_state(cmdBuf, outImage.get(), vk::PipelineStageFlagBits2::eBottomOfPipe, {}, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eColor);
    etna::flush_barriers(cmdBuf);
    ETNA_CHECK_VK_RESULT(cmdBuf.end());

    cmd_mgr.submitAndWait(std::move(cmdBuf));
    
    m_frequency *= 2;

    m_currentImage = 1 - m_currentImage;
    spdlog::info("Upscale to {} sucesfull", m_frequency);
}