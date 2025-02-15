#pragma once

#include "etna/ComputePipeline.hpp"
#include "pipelines/Pipeline.hpp"
#include "pipelines/RenderContext.hpp"

#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

#include "targets/Backbuffer.hpp"
#include "targets/Frame.hpp"


namespace pipes {

class TonemapPipeline {
public:
    using RenderTarget = targets::Frame;
    static_assert(RenderTarget::N_COLOR_ATTACHMENTS == 1, "Tonemap renders into single layer");

    TonemapPipeline() {}
    
    void allocate();
    
    void loadShaders();

    void setup();
    
    void drawGui();
    
    void debugInput(const Keyboard& /*kb*/);

    void render(vk::CommandBuffer cmd_buf, targets::Backbuffer& source, const RenderContext& ctx);

    bool enabled() const { return enable; }
    void tonemapEvaluate(vk::CommandBuffer cmd_buf, targets::Backbuffer& backbuffer, const RenderContext& ctx);
private: 
    
private:
    etna::GraphicsPipeline pipeline;
    etna::ComputePipeline histogramPipeline;
    etna::ComputePipeline distributionPipeline;

    etna::Sampler defaultSampler;
    
    etna::Buffer histogramBuffer;
    etna::Buffer distributionBuffer;

    bool enable = false;
};

}
static_assert(Pipeline<pipes::TonemapPipeline>, "Tonemap must be valid pipeline");