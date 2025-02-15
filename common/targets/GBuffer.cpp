#include "GBuffer.hpp"
#include <etna/GlobalContext.hpp>

namespace targets {

const std::vector<vk::Format> GBuffer::COLOR_ATTACHMENT_FORMATS = {
  vk::Format::eB8G8R8A8Unorm,
  vk::Format::eR16G16B16A16Snorm,
  vk::Format::eB8G8R8A8Unorm,
  vk::Format::eR32Sfloat,
};

void
GBuffer::allocate(glm::uvec2 extent)
{
  std::array<const char*, N_COLOR_ATTACHMENTS> names = {
    "gbuffer_albedo",
    "gbuffer_normal",
    "gbuffer_material",
    "gbuffer_wc",
  };
  resolution = extent;
  auto& depth = depth_buffer;

  auto& ctx = etna::get_context(); 

  for(std::size_t i = 0; i < N_COLOR_ATTACHMENTS; ++i) {
    color_buffer[i] = ctx.createImage({
      .extent = vk::Extent3D{resolution.x, resolution.y, 1},
      .name = names[i],
      .format = COLOR_ATTACHMENT_FORMATS[i],
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    });
  }

  depth = ctx.createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "gBuffer_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  color_attachments.resize(N_COLOR_ATTACHMENTS);

  for(std::size_t i = 0; i < N_COLOR_ATTACHMENTS; ++i) {
    color_attachments[i] = etna::RenderTargetState::AttachmentParams {
      .image = color_buffer[i].get(),
      .view  = color_buffer[i].getView({}),
      .imageAspect = vk::ImageAspectFlagBits::eColor,
    };
  }

  depth_attachment = etna::RenderTargetState::AttachmentParams {
    .image = depth.get(),
    .view  = depth.getView({}),
    .imageAspect = vk::ImageAspectFlagBits::eDepth,
  };
}

etna::Image&
GBuffer::getImage(std::size_t i)
{
  if (i == N_COLOR_ATTACHMENTS) {
    return depth_buffer;
  }
  return color_buffer[i];
}

}
