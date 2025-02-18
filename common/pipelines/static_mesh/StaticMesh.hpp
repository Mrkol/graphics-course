#pragma once

#include "pipelines/Pipeline.hpp"
#include "pipelines/RenderContext.hpp"

#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

#include "targets/GBuffer.hpp"


namespace pipes {

class StaticMeshPipeline {
public:
    using RenderTarget = targets::GBuffer;
    static_assert(RenderTarget::N_COLOR_ATTACHMENTS == 4, "StaticMesh renders into single layer");

    static const std::size_t N_MAX_INSTANCES = 1 << 14;

    StaticMeshPipeline() {}
    
    void allocate();
    
    void loadShaders();

    void setup();
    
    void drawGui();
    
    void debugInput(const Keyboard& /*kb*/);

    void reserve(std::size_t n);

    RenderTarget& render(vk::CommandBuffer cmd_buf, RenderTarget& target, const RenderContext& context);
private: 

    void prepareTextures(const RenderContext& context, vk::CommandBuffer);
    void prepareFrame(const RenderContext& context);

private:
    etna::GraphicsPipeline pipeline;
    etna::Buffer instanceMatricesBuf;
    etna::Buffer relemMaterialsBuf;
    etna::Sampler defaultSampler;
    struct PushConstants
    {
        glm::mat4x4 projView;
        glm::mat4x4 model;
        glm::vec4 color, emr_factors;
        glm::uint  relemIdx;
        glm::uint  material;
    } pushConst2M;

    std::vector<std::size_t> nInstances;

    bool normalMap = true;
    bool enableCulling = false;

    etna::DescriptorSet set2;
};

}
static_assert(Pipeline<pipes::StaticMeshPipeline>, "StaticMesh must be valid pipeline");