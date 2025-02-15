#pragma once

#include "pipelines/Pipeline.hpp"
#include "pipelines/RenderContext.hpp"

#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

#include "targets/Backbuffer.hpp"


namespace pipes {

class PerlinPipeline {
public:
    // using RenderTarget = targets::Backbuffer;
    // static_assert(RenderTarget::N_COLOR_ATTACHMENTS == 1, "Perlin renders into single layer");

    PerlinPipeline() {}
    
    void allocate();
    
    void loadShaders();

    void setup();
    
    void drawGui();
    
    void debugInput(const Keyboard& /*kb*/);

    etna::Image& getImage() { return m_images[m_currentImage]; }
    etna::Sampler& getSampler() { return m_sampler; }

    void render(etna::OneShotCmdMgr& cmd_mgr, int scale = 12);

    void reset() {
        m_currentImage = 0;
        m_frequency = 1;
    }
private: 
    void upscale(vk::CommandBuffer cmd_buf);
    
private:
    etna::GraphicsPipeline pipeline;
    etna::Image m_images[2];
    etna::Sampler m_sampler;

    vk::Extent2D m_extent;
    unsigned m_currentImage = 0;

    unsigned m_frequency = 1;
};

}
// static_assert(Pipeline<pipes::PerlinPipeline>, "Perlin must be valid pipeline");