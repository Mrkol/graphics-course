#pragma once

#include "pipelines/Pipeline.hpp"
#include "pipelines/RenderContext.hpp"

#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

#include "pipelines/perlin/Perlin.hpp"
#include "targets/GBuffer.hpp"
#include "targets/Buffer.hpp"


namespace pipes {

class TerrainPipeline {
public:
    using RenderTarget = targets::GBuffer;
    static_assert(RenderTarget::N_COLOR_ATTACHMENTS == 4, "Terrain renders into 4 layers");

    TerrainPipeline() {}
    
    void allocate();
    
    void loadShaders();

    void setup();
    
    void drawGui();
    
    void debugInput(const Keyboard& /*kb*/);

    RenderTarget& render(vk::CommandBuffer cmd_buf, RenderTarget& target, const RenderContext& context);
private: 
    void regenTerrain();



private:

    struct PushConstants {
        glm::vec2 base, extent;
        glm::mat4x4 mat; 
        glm::vec3 camPos;
        int degree;
    } pushConstants;

    etna::GraphicsPipeline pipeline;
    etna::GraphicsPipeline pipelineDebug;
    PerlinPipeline heightmapGenerator;

    int terrainScale = 12;
    bool wireframe = false;
};

}
static_assert(Pipeline<pipes::TerrainPipeline>, "Terrain must be valid pipeline");