#include "Backbuffer.hpp"

#include <etna/GlobalContext.hpp>

namespace targets {

const std::vector<vk::Format> Backbuffer::COLOR_ATTACHMENT_FORMATS = {vk::Format::eB10G11R11UfloatPack32};

void
Backbuffer::allocate(glm::uvec2 extent)
{
    resolution = extent;
    auto& ctx = etna::get_context();

    color_buffer = ctx.createImage(etna::Image::CreateInfo{
        .extent = vk::Extent3D{resolution.x, resolution.y, 1},
        .name = "backbuffer_color",
        .format = COLOR_ATTACHMENT_FORMATS[0],
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage,
    });

    depth_buffer = ctx.createImage(etna::Image::CreateInfo{
        .extent = vk::Extent3D{resolution.x, resolution.y, 1},
        .name = "backbuffer_depth",
        .format = DEPTH_ATTACHMENT_FORMAT,
        .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
    });


    colorAttachmentParams = {
        {
            .image = color_buffer.get(),
            .view = color_buffer.getView({}),
        }
    };
    
    depthAttachmentParams = etna::RenderTargetState::AttachmentParams{
        .image = depth_buffer.get(),
        .view = depth_buffer.getView({}),
    };
}

}
