#include "Skybox.hpp"

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
SkyboxPipeline::allocate()
{
    auto& ctx = etna::get_context();

    defaultSampler = etna::Sampler({
      .filter = vk::Filter::eLinear,
      .name = "perlinSample",
    });

    texture = ctx.createImage({
        .extent = vk::Extent3D{2048, 2048, 1},
        .name = "skybox",
        .format = vk::Format::eB8G8R8A8Srgb,
        .imageUsage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        .layers = 6,
        .flags = vk::ImageCreateFlagBits::eCubeCompatible,
    });

    for(size_t i = 0; i < 6; ++i) {
        int width, height, nChans;
        auto* imageBytes = stbi_load((GRAPHICS_COURSE_RESOURCES_ROOT "/textures/sb-" + std::to_string(i) + ".jpg").c_str(), &width, &height, &nChans, STBI_rgb_alpha);
        if (imageBytes == nullptr)
        {
            spdlog::log(spdlog::level::err, "Skybox load is unsuccessful");
            return;
        }
        size_t size = static_cast<std::size_t>(width * height * 4);
        etna::BlockingTransferHelper bth({.stagingSize = size});

        bth.uploadImage(*ctx.createOneShotCmdMgr(), texture, 0, i, std::span<const std::byte>(reinterpret_cast<const std::byte*>(imageBytes), size));
    }
}

void 
SkyboxPipeline::loadShaders() 
{
    etna::create_program(
        "skybox_shader",
        { SKYBOX_PIPELINE_SHADERS_ROOT "skybox.vert.spv",
          SKYBOX_PIPELINE_SHADERS_ROOT "skybox.frag.spv"}
    );
}

void 
SkyboxPipeline::setup() 
{
    auto& pipelineManager = etna::get_context().getPipelineManager();

    pipeline = pipelineManager.createGraphicsPipeline(
    "skybox_shader",
    etna::GraphicsPipeline::CreateInfo{
      .depthConfig = {
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        .maxDepthBounds = 1.f,
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
          .depthAttachmentFormat  = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
        },
    });
}

void 
SkyboxPipeline::drawGui()
{
    if(ImGui::TreeNode("skybox")) {
        ImGui::TreePop();
    }
}

void 
SkyboxPipeline::debugInput(const Keyboard& /*kb*/)
{

}

targets::Backbuffer& 
SkyboxPipeline::render(vk::CommandBuffer cmd_buf, targets::Backbuffer& target, const RenderContext& ctx)
{
  ETNA_PROFILE_GPU(cmd_buf, renderSkybox);
  {
    #if 0
    etna::set_state(cmd_buf, 
      gBuffer.back().get(), 
      vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests, 
      vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite, 
      vk::ImageLayout::eDepthStencilAttachmentOptimal, 
      vk::ImageAspectFlagBits::eDepth
    );
    etna::flush_barriers(cmd_buf);
    CustomRenderTargetState renderTargets(
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      {{.image = backbuffer.get(), .view = backbuffer.getView({}), .loadOp = vk::AttachmentLoadOp::eLoad,}},
      {.image = gBuffer.back().get(), .view = gBuffer.back().getView({}), .loadOp = vk::AttachmentLoadOp::eLoad, .storeOp = vk::AttachmentStoreOp::eStore, .layout=vk::ImageLayout::eDepthStencilAttachmentOptimal},
      {}
    );
    #endif

    auto skyboxShader = etna::get_shader_program("skybox_shader");

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());

    auto set = etna::create_descriptor_set(
      skyboxShader.getDescriptorLayoutId(0),
      cmd_buf,
      {
        etna::Binding{0, texture.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, {.layerCount=6, .type=vk::ImageViewType::eCube})}
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
