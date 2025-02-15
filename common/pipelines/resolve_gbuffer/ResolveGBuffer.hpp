#pragma once

#include "pipelines/Pipeline.hpp"
#include "pipelines/RenderContext.hpp"

#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

#include "targets/Backbuffer.hpp"
#include "targets/GBuffer.hpp"


namespace pipes {

class ResolveGBufferPipeline {
public:
    using RenderTarget = targets::Backbuffer;
    static_assert(RenderTarget::N_COLOR_ATTACHMENTS == 1, "ResolveGBuffer renders into single layer");

    ResolveGBufferPipeline() {}
    
    void allocate();
    
    void loadShaders();

    void setup();
    
    void drawGui();
    
    void debugInput(const Keyboard& /*kb*/);

    void render(vk::CommandBuffer cmd_buf, targets::GBuffer& source, const RenderContext& context, const etna::Image& skybox);
private: 
    void renderLights(vk::CommandBuffer cmd_buf);
    void renderSphereDeferred(vk::CommandBuffer cmd_buf, targets::GBuffer& source, const RenderContext& ctx);
    void renderSphere        (vk::CommandBuffer cmd_buf, const RenderContext& ctx);
private:
  etna::GraphicsPipeline deferredLightPipeline{};
  etna::GraphicsPipeline sphereDeferredPipeline{};
  etna::GraphicsPipeline spherePipeline{};
  etna::Sampler defaultSampler;
};

}
static_assert(Pipeline<pipes::ResolveGBufferPipeline>, "ResolveGBuffer must be valid pipeline");