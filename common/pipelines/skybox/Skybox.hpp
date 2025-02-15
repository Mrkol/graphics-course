#pragma once

#include "pipelines/Pipeline.hpp"
#include "pipelines/RenderContext.hpp"

#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

#include "targets/Backbuffer.hpp"


namespace pipes {

class SkyboxPipeline {
public:
    using RenderTarget = targets::Backbuffer;
    static_assert(RenderTarget::N_COLOR_ATTACHMENTS == 1, "Skybox renders into single layer");

    SkyboxPipeline() {}
    
    void allocate();
    
    void loadShaders();

    void setup();
    
    void drawGui();
    
    void debugInput(const Keyboard& /*kb*/);

    etna::Image& getImage() { return texture; }

    targets::Backbuffer& render(vk::CommandBuffer cmd_buf, targets::Backbuffer& target, const RenderContext& context);
private: 
    
private:
    etna::GraphicsPipeline pipeline;
    etna::Image texture;
    etna::Sampler defaultSampler;
};

}
static_assert(Pipeline<pipes::SkyboxPipeline>, "Skybox must be valid pipeline");