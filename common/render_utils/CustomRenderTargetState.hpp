#pragma once
#include <etna/RenderTargetStates.hpp>

class CustomRenderTargetState {
  vk::CommandBuffer commandBuffer;
//   static bool inScope;

public:
  struct AttachmentParams
  {
    vk::Image image = VK_NULL_HANDLE;
    vk::ImageView view = VK_NULL_HANDLE;
    std::optional<vk::ImageAspectFlags> imageAspect{};
    vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    vk::ClearColorValue clearColorValue = std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f});
    vk::ClearDepthStencilValue clearDepthStencilValue = {1.0f, 0};
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;

    // By default, the render target can work with multisample images and pipelines,
    // but not produce a final single-sample result.
    // These fields below are for the final MSAA image.
    // Ignore unless you know what MSAA is and aren't sure you need it.
    vk::Image resolveImage = VK_NULL_HANDLE;
    vk::ImageView resolveImageView = VK_NULL_HANDLE;
    std::optional<vk::ImageAspectFlags> resolveImageAspect{};
    vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone;
  };

  CustomRenderTargetState(
    vk::CommandBuffer cmd_buff,
    vk::Rect2D rect,
    const std::vector<AttachmentParams>& color_attachments,
    AttachmentParams depth_attachment,
    AttachmentParams stencil_attachment);
  ~CustomRenderTargetState();
  };

// bool CustomRenderTargetState::inScope = false;

CustomRenderTargetState::CustomRenderTargetState(
  vk::CommandBuffer cmd_buff,
  vk::Rect2D rect,
  const std::vector<AttachmentParams>& color_attachments,
  AttachmentParams depth_attachment,
  AttachmentParams stencil_attachment)
{
//   ETNA_VERIFYF(!inScope, "RenderTargetState scopes shouldn't overlap.");
//   inScope = true;
  commandBuffer = cmd_buff;
  vk::Viewport viewport{
    .x = static_cast<float>(rect.offset.x),
    .y = static_cast<float>(rect.offset.y),
    .width = static_cast<float>(rect.extent.width),
    .height = static_cast<float>(rect.extent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  commandBuffer.setViewport(0, {viewport});
  commandBuffer.setScissor(0, {rect});

  std::vector<vk::RenderingAttachmentInfo> attachmentInfos(color_attachments.size());
  for (uint32_t i = 0; i < color_attachments.size(); ++i)
  {
    attachmentInfos[i].imageView = color_attachments[i].view;
    attachmentInfos[i].imageLayout = (color_attachments[i].layout != vk::ImageLayout::eUndefined) ?  color_attachments[i].layout : vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfos[i].loadOp = color_attachments[i].loadOp;
    attachmentInfos[i].storeOp = color_attachments[i].storeOp;
    attachmentInfos[i].clearValue = color_attachments[i].clearColorValue;
  }

  vk::RenderingAttachmentInfo depthAttInfo{
    .imageView = depth_attachment.view,
    .imageLayout = (depth_attachment.layout != vk::ImageLayout::eUndefined) ? depth_attachment.layout : vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .resolveMode = depth_attachment.resolveMode,
    .resolveImageView = depth_attachment.resolveImageView,
    .resolveImageLayout = vk::ImageLayout::eGeneral,
    .loadOp = depth_attachment.loadOp,
    .storeOp = depth_attachment.storeOp,
    .clearValue = depth_attachment.clearDepthStencilValue,
  };

  vk::RenderingAttachmentInfo stencilAttInfo{
    .imageView = stencil_attachment.view,
    .imageLayout = (stencil_attachment.layout != vk::ImageLayout::eUndefined) ? stencil_attachment.layout : vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .resolveMode = stencil_attachment.resolveMode,
    .resolveImageView = stencil_attachment.resolveImageView,
    .resolveImageLayout = vk::ImageLayout::eGeneral,
    .loadOp = stencil_attachment.loadOp,
    .storeOp = stencil_attachment.storeOp,
    .clearValue = stencil_attachment.clearDepthStencilValue,
  };

  vk::RenderingInfo renderInfo{
    .renderArea = rect,
    .layerCount = 1,
    .colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size()),
    .pColorAttachments = attachmentInfos.empty() ? nullptr : attachmentInfos.data(),
    .pDepthAttachment = depth_attachment.view ? &depthAttInfo : nullptr,
    .pStencilAttachment = stencil_attachment.view ? &stencilAttInfo : nullptr,
  };
  commandBuffer.beginRendering(renderInfo);
}

CustomRenderTargetState::~CustomRenderTargetState()
{
  commandBuffer.endRendering();
//   inScope = false;
}