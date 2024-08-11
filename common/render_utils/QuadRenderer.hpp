#pragma once

#include <etna/Vulkan.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>


/**
 * Simple class for displaying a texture on the screen for debug purposes.
 */
class QuadRenderer
{
public:
  struct CreateInfo
  {
    vk::Format format = vk::Format::eUndefined;
    vk::Rect2D rect = {};
  };

  explicit QuadRenderer(CreateInfo info);
  ~QuadRenderer() {}

  void render(
    vk::CommandBuffer cmd_buff,
    vk::Image target_image,
    vk::ImageView target_image_view,
    const etna::Image& tex_to_draw,
    const etna::Sampler& sampler);

private:
  etna::GraphicsPipeline pipeline;
  etna::ShaderProgramId programId;
  vk::Rect2D rect{};

  QuadRenderer(const QuadRenderer&) = delete;
  QuadRenderer& operator=(const QuadRenderer&) = delete;
};
