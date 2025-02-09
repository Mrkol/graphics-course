#ifndef TERRAIN_RENDERER_PERLIN_HPP
#define TERRAIN_RENDERER_PERLIN_HPP

#include <etna/PipelineManager.hpp>
#include <etna/OneShotCmdMgr.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

class PerlinGenerator
{
public:
    PerlinGenerator() {}
    
    void initImage(vk::Extent2D extent);

    void upscale(etna::OneShotCmdMgr& cmd_mgr);
    void reset() {
        m_currentImage = 0;
        m_frequency = 1;
    }

    etna::Image& getImage() { return m_images[m_currentImage]; }
    etna::Sampler& getSampler() { return m_sampler; }
private:

    etna::Image m_images[2];
    etna::Sampler m_sampler;

    etna::GraphicsPipeline m_pipeline;
    unsigned m_currentImage = 0;

    unsigned m_frequency = 1;
    vk::Extent2D m_extent;
};

#endif /* TERRAIN_RENDERER_PERLIN_HPP */
