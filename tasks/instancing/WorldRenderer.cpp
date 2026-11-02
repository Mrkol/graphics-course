#include "WorldRenderer.hpp"
#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <imgui.h>
#include <cstring>
#include <algorithm>
#include <vector>

void WorldRenderer::resizeDepth(glm::uvec2 res)
{
  resolution = res;
  depth = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{res.x, res.y, 1},
    .name = "terrain_depth",
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
  });
}

void WorldRenderer::allocateResources(glm::uvec2 res)
{
  resizeDepth(res);
  auto& ctx = etna::get_context();
  heightMap = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{1024, 1024, 1},
    .name = "terrain_height_R32_SFLOAT",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
  });
  albedoAtlas = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{1024, 256, 1},
    .name = "PBR_albedo_sand_grass_rock_snow",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
  });
  roughnessAtlas = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{1024, 256, 1},
    .name = "PBR_roughness_sand_grass_rock_snow",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
  });
  detailHeightAtlas = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{1024, 256, 1},
    .name = "PBR_detail_height_sand_grass_rock_snow",
    .format = vk::Format::eR32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
  });
  sampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "terrain_linear_sampler"});

  std::vector<uint32_t> meshIndices;
  meshIndices.reserve(kIndexCount);
  for (uint32_t y = 0; y < kGridCells; ++y)
    for (uint32_t x = 0; x < kGridCells; ++x)
    {
      const uint32_t a = y * (kGridCells + 1) + x;
      const uint32_t b = a + 1;
      const uint32_t c = a + kGridCells + 1;
      const uint32_t d = c + 1;
      meshIndices.insert(meshIndices.end(), {a, c, b, b, c, d});
    }
  indices = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = static_cast<vk::DeviceSize>(meshIndices.size() * sizeof(uint32_t)),
    .bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .allocationCreate =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = "terrain_256x256_index_grid",
  });
  std::memcpy(indices.data(), meshIndices.data(), meshIndices.size() * sizeof(uint32_t));
  rocks.allocateResources();
  pendingGeneration = true;
}

void WorldRenderer::loadShaders()
{
  rocks.loadShaders();
  etna::create_program("terrain_generate", {INSTANCING_SHADERS_ROOT "generate.comp.spv"});
  etna::create_program("terrain_surface",
                       {INSTANCING_SHADERS_ROOT "terrain.frag.spv",
                        INSTANCING_SHADERS_ROOT "terrain.vert.spv"});
}

void WorldRenderer::setupPipelines(vk::Format swapchain_format)
{
  auto& manager = etna::get_context().getPipelineManager();
  generatePipeline = manager.createComputePipeline("terrain_generate", {});
  rocks.setupPipeline(swapchain_format);
  drawPipeline = manager.createGraphicsPipeline("terrain_surface", etna::GraphicsPipeline::CreateInfo{
    .rasterizationConfig = vk::PipelineRasterizationStateCreateInfo{
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .lineWidth = 1.0f,
    },
    .fragmentShaderOutput = {
      .colorAttachmentFormats = {swapchain_format},
      .depthAttachmentFormat = vk::Format::eD32Sfloat,
    },
  });
}

void WorldRenderer::debugInput(const Keyboard&) {}

void WorldRenderer::update(const FramePacket& packet)
{
  const float aspect = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
  draw.viewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
  draw.eye = glm::vec4(packet.mainCam.position, 0.f);
  rocks.update(draw, noise);
}

void WorldRenderer::drawGui()
{
  ImGui::Begin("Terrain");
  ImGui::SeparatorText("Noise");
  ImGui::SliderInt("Octaves", &noise.octaves, 1, 10);
  ImGui::SliderFloat("Scale", &noise.scale, 0.5f, 12.f);
  ImGui::SliderFloat("Persistence", &noise.persistence, 0.2f, 0.85f);
  ImGui::SliderFloat("Lacunarity", &noise.lacunarity, 1.5f, 3.5f);
  if (ImGui::Button("Regenerate"))
  {
    pendingGeneration = true;
    rocks.invalidate();
  }

  ImGui::SeparatorText("Lighting");
  ImGui::SliderFloat3("Light position", &draw.light.x, -100.f, 100.f);
  ImGui::SliderFloat("Light intensity", &draw.light.w, 0.1f, 20.f);

  ImGui::SeparatorText("Materials");
  ImGui::SliderFloat("Texture tiling", &draw.detail.x, 2.f, 40.f);
  ImGui::SliderFloat("Height detail", &draw.detail.y, 0.f, 2.5f);
  rocks.drawGui();
  ImGui::End();
}

void WorldRenderer::generateMaps(vk::CommandBuffer cmd)
{
  const auto toStorage = [&](etna::Image& image) {
    etna::set_state(cmd, image.get(), vk::PipelineStageFlagBits2::eComputeShader,
                    vk::AccessFlagBits2::eShaderStorageWrite,
                    vk::ImageLayout::eGeneral, vk::ImageAspectFlagBits::eColor);
  };
  toStorage(heightMap);
  toStorage(albedoAtlas);
  toStorage(roughnessAtlas);
  toStorage(detailHeightAtlas);
  etna::flush_barriers(cmd);

  auto shader = etna::get_shader_program("terrain_generate");
  auto set = etna::create_descriptor_set(shader.getDescriptorLayoutId(0), cmd, {
    etna::Binding{0, heightMap.genBinding(vk::Sampler{}, vk::ImageLayout::eGeneral)},
    etna::Binding{1, albedoAtlas.genBinding(vk::Sampler{}, vk::ImageLayout::eGeneral)},
    etna::Binding{2, roughnessAtlas.genBinding(vk::Sampler{}, vk::ImageLayout::eGeneral)},
    etna::Binding{3, detailHeightAtlas.genBinding(vk::Sampler{}, vk::ImageLayout::eGeneral)},
  });
  cmd.bindPipeline(vk::PipelineBindPoint::eCompute, generatePipeline.getVkPipeline());
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                         generatePipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});
  cmd.pushConstants(generatePipeline.getVkPipelineLayout(),
                    vk::ShaderStageFlagBits::eCompute, 0, sizeof(noise), &noise);
  cmd.dispatch(32, 32, 1);

  const auto toSampled = [&](etna::Image& image) {
    etna::set_state(cmd, image.get(),
                    vk::PipelineStageFlagBits2::eVertexShader |
                      vk::PipelineStageFlagBits2::eFragmentShader,
                    vk::AccessFlagBits2::eShaderSampledRead,
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    vk::ImageAspectFlagBits::eColor);
  };
  toSampled(heightMap);
  toSampled(albedoAtlas);
  toSampled(roughnessAtlas);
  toSampled(detailHeightAtlas);
  etna::flush_barriers(cmd);
  pendingGeneration = false;
}

void WorldRenderer::renderWorld(vk::CommandBuffer cmd, vk::Image target, vk::ImageView view, uint32_t frameSlot)
{
  if (pendingGeneration) generateMaps(cmd);

  auto shader = etna::get_shader_program("terrain_surface");
  auto set = etna::create_descriptor_set(shader.getDescriptorLayoutId(0), cmd, {
    etna::Binding{0, heightMap.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    etna::Binding{1, albedoAtlas.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    etna::Binding{2, roughnessAtlas.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    etna::Binding{3, detailHeightAtlas.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
  });
  etna::RenderTargetState renderTargets(
    cmd,
    {{0, 0}, {resolution.x, resolution.y}},
    {{.image = target, .view = view}},
    {.image = depth.get(), .view = depth.getView({})});

  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, drawPipeline.getVkPipeline());
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                         drawPipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});
  cmd.pushConstants(drawPipeline.getVkPipelineLayout(),
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                    0, sizeof(draw), &draw);
  cmd.bindIndexBuffer(indices.get(), 0, vk::IndexType::eUint32);
  cmd.drawIndexed(kIndexCount, 1, 0, 0, 0);
  rocks.render(cmd, heightMap, sampler, draw, frameSlot);
}
