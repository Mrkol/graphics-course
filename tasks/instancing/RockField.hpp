#pragma once
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "RenderSettings.hpp"

class RockField
{
public:
  void allocateResources();
  void loadShaders();
  void setupPipeline(vk::Format format);
  void update(const DrawSettings& draw, const NoiseSettings& noise);
  void invalidate();
  void drawGui();
  void render(vk::CommandBuffer cmd, const etna::Image& heightMap,
              const etna::Sampler& sampler, const DrawSettings& draw, uint32_t frameSlot);

private:
  struct Vertex
  {
    glm::vec4 position;
    glm::vec4 normal;
  };

  struct Instance
  {
    glm::mat4 transform;
    glm::vec4 color;
  };

  struct Candidate
  {
    Instance instance;
    glm::vec3 center;
    float radius;
    uint32_t model;
  };

  struct Mesh
  {
    etna::Buffer vertices;
    etna::Buffer indices;
    std::array<etna::Buffer, 2> instances;
    uint32_t indexCount = 0;
    float radius = 1.f;
  };

  struct Chunk
  {
    std::vector<Candidate> candidates;
  };

  void loadMesh(uint32_t model, const char* path);
  void generateChunk(int x, int z);
  void updateVisible(const DrawSettings& draw);
  float heightAt(glm::vec2 uv) const;

  static constexpr uint32_t kMaxInstances = 12000;
  static constexpr int kChunkCells = 6;
  static constexpr float kCellSize = 1.5f;
  static constexpr float kChunkSize = kChunkCells * kCellSize;

  std::array<Mesh, 2> meshes;
  std::array<std::vector<Instance>, 2> visible;
  std::unordered_map<int64_t, Chunk> chunks;
  etna::GraphicsPipeline pipeline{};
  NoiseSettings noise{};
  glm::vec3 cameraPos{0.f};
  float density = 0.28f;
  float drawDistance = 68.f;
  float minScale = 0.65f;
  float maxScale = 2.2f;
  bool dirty = true;
  bool noiseDirty = true;
};
