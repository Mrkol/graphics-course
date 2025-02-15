#pragma once
#include "etna/Image.hpp"
#include "etna/RenderTargetStates.hpp"
#include <glm/glm.hpp>
namespace targets {

class Frame {
public:
    static const std::vector<vk::Format> COLOR_ATTACHMENT_FORMATS;// = {vk::Format::eB10G11R11UfloatPack32};
    static const vk::Format              DEPTH_ATTACHMENT_FORMAT  = vk::Format::eUndefined;
    static const constexpr int N_COLOR_ATTACHMENTS = 1;

    const std::vector<etna::RenderTargetState::AttachmentParams>& getColorAttachments();
    etna::RenderTargetState::AttachmentParams getDepthAttachment();
    
    glm::uvec2 getResolution();

    void allocate();

private:
    etna::Image buffer;
};

}