#pragma once
#include "etna/Image.hpp"
#include "etna/RenderTargetStates.hpp"
#include <glm/glm.hpp>
namespace targets {

class Backbuffer {
public:
    static const std::vector<vk::Format> COLOR_ATTACHMENT_FORMATS;// = {vk::Format::eB10G11R11UfloatPack32};
    static const vk::Format              DEPTH_ATTACHMENT_FORMAT  = vk::Format::eD32Sfloat;
    static const constexpr int N_COLOR_ATTACHMENTS = 1;

    const std::vector<etna::RenderTargetState::AttachmentParams>& getColorAttachments() { return colorAttachmentParams; }
    
    etna::RenderTargetState::AttachmentParams getDepthAttachment() { return depthAttachmentParams; }
    
    glm::uvec2 getResolution() { return resolution; }

    void allocate(glm::uvec2 resolution);

    vk::Image get() { return color_buffer.get(); }

    etna::ImageBinding genBinding(vk::Sampler sampler, vk::ImageLayout layout) {return color_buffer.genBinding(sampler, layout);}



private:
    etna::Image color_buffer;
    etna::Image depth_buffer;
    glm::uvec2 resolution{};

    std::vector<etna::RenderTargetState::AttachmentParams> colorAttachmentParams;
    etna::RenderTargetState::AttachmentParams depthAttachmentParams;
};

}