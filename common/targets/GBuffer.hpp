#pragma once
#include "etna/DescriptorSet.hpp"
#include "etna/Image.hpp"
#include "etna/RenderTargetStates.hpp"
#include <glm/glm.hpp>
namespace targets {

class GBuffer {
public:
    static const std::vector<vk::Format> COLOR_ATTACHMENT_FORMATS;// = {vk::Format::eB10G11R11UfloatPack32};
    static const vk::Format              DEPTH_ATTACHMENT_FORMAT  = vk::Format::eD32Sfloat;
    static const constexpr int N_COLOR_ATTACHMENTS = 4;

    const std::vector<etna::RenderTargetState::AttachmentParams>& getColorAttachments() { return color_attachments; }
    etna::RenderTargetState::AttachmentParams getDepthAttachment() { return depth_attachment; }
    
    glm::uvec2 getResolution();

    etna::Image& getImage(std::size_t i);
    etna::Image& getDepthImage() { return depth_buffer; }

    void allocate(glm::uvec2 extent);

private:
    std::array<etna::Image, N_COLOR_ATTACHMENTS> color_buffer;

    std::vector<etna::RenderTargetState::AttachmentParams> color_attachments;
    etna::RenderTargetState::AttachmentParams depth_attachment;
    
    etna::Image depth_buffer;
    glm::uvec2 resolution;
};

}