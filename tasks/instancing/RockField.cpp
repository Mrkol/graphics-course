#include "RockField.hpp"
#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/PipelineManager.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{
uint32_t mixBits(uint32_t n)
{
  n ^= n >> 16;
  n *= 0x7feb352du;
  n ^= n >> 15;
  n *= 0x846ca68bu;
  return n ^ (n >> 16);
}

uint32_t hashCell(int x, int y, uint32_t salt)
{
  return mixBits(uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + salt);
}

float random01(int x, int y, uint32_t salt)
{
  return float(hashCell(x, y, salt) >> 8) * (1.f / 16777216.f);
}

float mixf(float a, float b, float t)
{
  return a + (b - a) * t;
}

float smooth(float a, float b, float x)
{
  const float t = glm::clamp((x - a) / (b - a), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

float smoothNoise(glm::vec2 p, int seed)
{
  const int x = int(std::floor(p.x));
  const int y = int(std::floor(p.y));
  glm::vec2 f = p - glm::vec2(x, y);
  f = f * f * (3.f - 2.f * f);
  const auto rnd = [seed](int a, int b) {
    uint32_t h = (uint32_t(a) * 374761393u + uint32_t(b) * 668265263u) ^ uint32_t(seed);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return float(h) / 4294967295.f;
  };
  return mixf(mixf(rnd(x, y), rnd(x + 1, y), f.x),
              mixf(rnd(x, y + 1), rnd(x + 1, y + 1), f.x), f.y);
}

float fbm(glm::vec2 p, int count, float persistence, float lacunarity, int seed)
{
  float sum = 0.f;
  float amp = 0.5f;
  float total = 0.f;
  for (int i = 0; i < count; ++i)
  {
    sum += smoothNoise(p, seed) * amp;
    total += amp;
    p *= lacunarity;
    amp *= persistence;
  }
  return sum / std::max(total, 0.00001f);
}

int64_t chunkKey(int x, int z)
{
  return int64_t((uint64_t(uint32_t(x)) << 32) | uint32_t(z));
}

std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& m)
{
  const auto row = [&m](int r) { return glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]); };
  const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
  std::array<glm::vec4, 6> planes{r3 + r0, r3 - r0, r3 + r1,
                                  r3 - r1, r2, r3 - r2};
  for (auto& p : planes)
    p /= std::max(glm::length(glm::vec3(p)), 0.00001f);
  return planes;
}

bool sphereVisible(const std::array<glm::vec4, 6>& planes, glm::vec3 center, float radius)
{
  for (const auto& p : planes)
    if (glm::dot(glm::vec3(p), center) + p.w < -radius)
      return false;
  return true;
}
}

void RockField::loadMesh(uint32_t model, const char* name)
{
  std::ifstream file(std::string(INSTANCING_ASSETS_ROOT) + name, std::ios::binary);
  if (!file) throw std::runtime_error(std::string("Cannot open rock mesh: ") + name);

  char magic[4]{};
  uint32_t vertexCount = 0, indexCount = 0;
  file.read(magic, 4);
  file.read(reinterpret_cast<char*>(&vertexCount), 4);
  file.read(reinterpret_cast<char*>(&indexCount), 4);
  if (std::memcmp(magic, "RCK1", 4) != 0 || !vertexCount || !indexCount ||
      vertexCount > 2000000 || indexCount > 6000000)
    throw std::runtime_error(std::string("Invalid rock mesh: ") + name);

  std::vector<Vertex> vertices(vertexCount);
  std::vector<uint32_t> indices(indexCount);
  file.read(reinterpret_cast<char*>(vertices.data()), std::streamsize(vertices.size() * sizeof(Vertex)));
  file.read(reinterpret_cast<char*>(indices.data()), std::streamsize(indices.size() * sizeof(uint32_t)));
  if (!file) throw std::runtime_error(std::string("Incomplete rock mesh: ") + name);

  auto& mesh = meshes[model];
  auto& ctx = etna::get_context();
  mesh.radius = 0.f;
  for (const auto& vertex : vertices)
    mesh.radius = std::max(mesh.radius, glm::length(glm::vec3(vertex.position)));
  mesh.indexCount = indexCount;
  mesh.vertices = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = vk::DeviceSize(vertices.size() * sizeof(Vertex)),
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = name,
  });
  mesh.indices = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = vk::DeviceSize(indices.size() * sizeof(uint32_t)),
    .bufferUsage = vk::BufferUsageFlagBits::eIndexBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
    .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT,
    .name = name,
  });
  std::memcpy(mesh.vertices.data(), vertices.data(), vertices.size() * sizeof(Vertex));
  std::memcpy(mesh.indices.data(), indices.data(), indices.size() * sizeof(uint32_t));
  for (uint32_t frame = 0; frame < 2; ++frame)
    mesh.instances[frame] = ctx.createBuffer(etna::Buffer::CreateInfo{
      .size = vk::DeviceSize(kMaxInstances * sizeof(Instance)),
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
      .name = "rock_instances",
    });
}

void RockField::allocateResources()
{
  loadMesh(0, "boulder_01.mesh");
  loadMesh(1, "rock_09.mesh");
  for (auto& group : visible) group.reserve(kMaxInstances);
}

void RockField::loadShaders()
{
  etna::create_program("instanced_rocks", {INSTANCING_SHADERS_ROOT "rocks.frag.spv",
                                          INSTANCING_SHADERS_ROOT "rocks.vert.spv"});
}

void RockField::setupPipeline(vk::Format format)
{
  pipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
    "instanced_rocks", etna::GraphicsPipeline::CreateInfo{
      .rasterizationConfig = vk::PipelineRasterizationStateCreateInfo{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.f,
      },
      .fragmentShaderOutput = {
        .colorAttachmentFormats = {format},
        .depthAttachmentFormat = vk::Format::eD32Sfloat,
      },
    });
}

float RockField::heightAt(glm::vec2 uv) const
{
  const float coastX = fbm(uv * 5.4f + glm::vec2(18.4f, 3.1f), 3, 0.5f, 2.f, noise.seed) - 0.5f;
  const float coastY = fbm(uv * 5.4f + glm::vec2(3.3f, 13.1f), 3, 0.5f, 2.f, noise.seed) - 0.5f;
  const glm::vec2 p = (uv * 2.f - 1.f) * glm::vec2(1.05f, 0.91f) +
                      glm::vec2(coastX, coastY) * 0.17f;
  const float island = 1.f - smooth(noise.islandRadius - 0.30f,
                                     noise.islandRadius, glm::length(p));
  const float hills = fbm(uv * noise.scale + glm::vec2(17.4f, 6.8f),
                          noise.octaves, noise.persistence, noise.lacunarity, noise.seed);
  const float smallHills = fbm(uv * noise.scale * 2.8f + glm::vec2(1.3f, 8.1f),
                               4, 0.52f, 2.05f, noise.seed);
  const glm::vec2 a = (uv - glm::vec2(0.38f, 0.51f)) * glm::vec2(6.2f, 6.8f);
  const glm::vec2 b = (uv - glm::vec2(0.64f, 0.41f)) * glm::vec2(9.2f, 6.3f);
  const glm::vec2 c = (uv - glm::vec2(0.57f, 0.70f)) * glm::vec2(11.f, 10.5f);
  const float peaks = 7.8f * std::exp(-glm::dot(a, a)) +
                      6.1f * std::exp(-glm::dot(b, b)) +
                      2.6f * std::exp(-glm::dot(c, c));
  return (island * (2.f + 3.3f * hills + 1.6f * smallHills + peaks) - noise.shore) *
         noise.heightScale;
}

void RockField::generateChunk(int cx, int cz)
{
  Chunk chunk;
  chunk.candidates.reserve(kChunkCells * kChunkCells);
  for (int z = 0; z < kChunkCells; ++z)
    for (int x = 0; x < kChunkCells; ++x)
    {
      const int gx = cx * kChunkCells + x;
      const int gz = cz * kChunkCells + z;
      if (random01(gx, gz, 0x983abc21u) > std::min(density, 1.f)) continue;
      const float wx = (float(gx) + random01(gx, gz, 0x120ee35au)) * kCellSize;
      const float wz = (float(gz) + random01(gx, gz, 0xaef04c32u)) * kCellSize;
      if (wx < -49.f || wx > 49.f || wz < -49.f || wz > 49.f) continue;
      const float height = heightAt(glm::vec2(wx, wz) / 100.f + 0.5f);
      const float probability = smooth(0.3f, 3.4f, height) *
                                (1.f - smooth(8.5f, 11.5f, height));
      if (random01(gx, gz, 0xcaa931d7u) > probability) continue;
      const uint32_t model = random01(gx, gz, 0x9419e0bfu) < 0.5f ? 0u : 1u;
      const float s = mixf(minScale, maxScale, random01(gx, gz, 0x5ee57d4cu));
      const float angle = random01(gx, gz, 0x3563aaa1u) * 6.2831853f;
      const float tilt = (random01(gx, gz, 0xfd3119a2u) - 0.5f) * 0.26f;
      glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(wx, -0.1f * s, wz));
      transform = glm::rotate(transform, angle, glm::vec3(0.f, 1.f, 0.f));
      transform = glm::rotate(transform, tilt, glm::vec3(1.f, 0.f, 0.f));
      transform = glm::scale(transform, glm::vec3(s));
      const float tint = 0.82f + 0.35f * random01(gx, gz, 0x4ae1f05bu);
      const glm::vec3 base = model == 0u ? glm::vec3(0.43f, 0.42f, 0.39f) :
                                            glm::vec3(0.48f, 0.35f, 0.29f);
      const glm::vec3 pos(wx, height + 0.45f * s, wz);
      chunk.candidates.push_back(Candidate{
        .instance = Instance{transform, glm::vec4(base * tint, 0.77f +
                            0.2f * random01(gx, gz, 0x1f88030du))},
        .center = pos,
        .radius = meshes[model].radius * s + 0.5f,
        .model = model,
      });
    }
  chunks.emplace(chunkKey(cx, cz), std::move(chunk));
}

void RockField::updateVisible(const DrawSettings& draw)
{
  for (auto& list : visible) list.clear();
  const auto planes = frustumPlanes(draw.viewProj);
  for (const auto& [key, chunk] : chunks)
    for (const auto& candidate : chunk.candidates)
    {
      const glm::vec2 d = glm::vec2(candidate.center.x - cameraPos.x,
                                    candidate.center.z - cameraPos.z);
      const float range = drawDistance + candidate.radius;
      if (glm::dot(d, d) > range * range ||
          !sphereVisible(planes, candidate.center, candidate.radius))
        continue;
      auto& list = visible[candidate.model];
      if (list.size() < kMaxInstances) list.push_back(candidate.instance);
    }
}

void RockField::update(const DrawSettings& draw, const NoiseSettings& terrainNoise)
{
  cameraPos = glm::vec3(draw.eye);
  if (noiseDirty)
  {
    noise = terrainNoise;
    noiseDirty = false;
    dirty = true;
  }
  if (dirty)
  {
    chunks.clear();
    dirty = false;
  }
  const int centerX = int(std::floor(cameraPos.x / kChunkSize));
  const int centerZ = int(std::floor(cameraPos.z / kChunkSize));
  const int radius = int(std::ceil(drawDistance / kChunkSize)) + 1;
  for (int z = centerZ - radius; z <= centerZ + radius; ++z)
    for (int x = centerX - radius; x <= centerX + radius; ++x)
    {
      const float nx = glm::clamp(cameraPos.x, x * kChunkSize, (x + 1) * kChunkSize);
      const float nz = glm::clamp(cameraPos.z, z * kChunkSize, (z + 1) * kChunkSize);
      const float dx = cameraPos.x - nx, dz = cameraPos.z - nz;
      if (dx * dx + dz * dz > drawDistance * drawDistance) continue;
      if (chunks.find(chunkKey(x, z)) == chunks.end()) generateChunk(x, z);
    }
  for (auto it = chunks.begin(); it != chunks.end();)
  {
    const int x = int(int32_t(uint64_t(it->first) >> 32));
    const int z = int(int32_t(uint32_t(it->first)));
    if (std::abs(x - centerX) > radius + 1 || std::abs(z - centerZ) > radius + 1)
      it = chunks.erase(it);
    else ++it;
  }
  updateVisible(draw);
}

void RockField::invalidate()
{
  noiseDirty = true;
}

void RockField::drawGui()
{
  ImGui::SeparatorText("Rocks");
  dirty |= ImGui::SliderFloat("Density", &density, 0.f, 1.f);
  dirty |= ImGui::SliderFloat("Draw distance", &drawDistance, 10.f, 110.f);
  bool changed = ImGui::SliderFloat("Minimum size", &minScale, 0.2f, 3.f);
  changed |= ImGui::SliderFloat("Maximum size", &maxScale, 0.3f, 5.f);
  if (minScale > maxScale) maxScale = minScale;
  dirty |= changed;
  ImGui::Text("Visible rocks: %zu + %zu", visible[0].size(), visible[1].size());
}

void RockField::render(vk::CommandBuffer cmd, const etna::Image& heightMap,
                       const etna::Sampler& sampler, const DrawSettings& draw,
                       uint32_t frameSlot)
{
  auto shader = etna::get_shader_program("instanced_rocks");
  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
  cmd.pushConstants(pipeline.getVkPipelineLayout(),
                    vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                    0, sizeof(draw), &draw);
  for (uint32_t model = 0; model < meshes.size(); ++model)
  {
    if (visible[model].empty()) continue;
    auto& mesh = meshes[model];
    std::memcpy(mesh.instances[frameSlot].data(), visible[model].data(),
                visible[model].size() * sizeof(Instance));
    auto descriptor = etna::create_descriptor_set(shader.getDescriptorLayoutId(0), cmd, {
      etna::Binding{0, mesh.vertices.genBinding()},
      etna::Binding{1, mesh.instances[frameSlot].genBinding()},
      etna::Binding{2, heightMap.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
    });
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipelineLayout(),
                           0, {descriptor.getVkSet()}, {});
    cmd.bindVertexBuffers(0, {mesh.vertices.get()}, {0});
    cmd.bindIndexBuffer(mesh.indices.get(), 0, vk::IndexType::eUint32);
    cmd.drawIndexed(mesh.indexCount, uint32_t(visible[model].size()), 0, 0, 0);
  }
}
