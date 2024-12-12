#pragma once

#include "etna/Buffer.hpp"
#include "etna/OneShotCmdMgr.hpp"
#include "shaders/UniformParams.h"
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <glm/glm.hpp>

const size_t NUM_FRAMES_IN_FLIGHT = 2;

struct RenderParams {
    glm::vec2 resolution;
};

class Renderer
{
public: 
    Renderer() {}
    ~Renderer() {}

    void initPipelines(glm::uvec2 resolution, vk::Format swapchain_format);

    void loadResource(etna::OneShotCmdMgr& cmd_mgr, const char* name);

    void render(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view, uint32_t constants_size, void* constants);

    void update(glm::vec3 cam_view);

private:
    std::vector<etna::Image> textures;
    etna::Sampler defaultSampler;
    
    std::array<etna::Buffer, 2> constants;
    etna::GraphicsPipeline basePipeline;

    etna::Image skybox;
    etna::Sampler sbSampler;
    etna::GraphicsPipeline skyboxPipeline;

    void initView();

    glm::uvec2 resolution;
    UniformParams params;
    size_t nFrame = 0;
};