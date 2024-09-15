#pragma once

#include <glm/glm.hpp>

#include <etna/Vulkan.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>


class LineRenderer
{
public:
  struct Vertex
  {
    glm::vec3 position;
    glm::vec3 color;
  };

  struct CreateInfo
  {
    vk::Format format = vk::Format::eUndefined;

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
  };

  explicit LineRenderer(CreateInfo info);
  ~LineRenderer() {}

  void render(
    vk::CommandBuffer cmd_buf,
    vk::Rect2D rect,
    glm::mat4 proj_view,
    vk::Image target_image,
    vk::ImageView target_image_view,
    vk::Image depth_image,
    vk::ImageView depth_image_view);

private:
  etna::GraphicsPipeline pipeline;
  etna::ShaderProgramId programId;

  etna::Buffer vertexBuffer;
  etna::Buffer indexBuffer;

  std::size_t indexCount;

  LineRenderer(const LineRenderer&) = delete;
  LineRenderer& operator=(const LineRenderer&) = delete;
};
