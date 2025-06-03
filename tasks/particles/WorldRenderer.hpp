#pragma once

#include <etna/Image.hpp>
#include <etna/Sampler.hpp>
#include <etna/Buffer.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <glm/glm.hpp>

#include "scene/SceneManager.hpp"
#include "wsi/Keyboard.hpp"

#include "FramePacket.hpp"


struct PlaceholderTextureManager {
  std::vector<etna::Image> textures;

  PlaceholderTextureManager(vk::CommandBuffer cmd_buf);
};


class WorldRenderer
{
public:
  WorldRenderer();

  // If a scene is already loaded, please unbind the current scene before loading another.
  // This is two separate functions because a cmd buffer is necessary for the operation.
  // Maybe OneShotCmdBuf can be used for that, didn't try it.
  void loadScene(std::filesystem::path path);
  void unbindScene(vk::CommandBuffer cmd_buf);

  void loadShaders();
  void allocateResources(glm::uvec2 swapchain_resolution);
  void setupPipelines(vk::Format swapchain_format);

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawGui();
  void renderWorld(
    vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view);

private:
  void refresh_textures(vk::CommandBuffer cmd_buf);
  void renderScene(
    vk::CommandBuffer cmd_buf, const glm::mat4x4& glob_tm, vk::PipelineLayout pipeline_layout);


private:
  std::unique_ptr<etna::OneShotCmdMgr> oneShotCommands;
  etna::BlockingTransferHelper transferHelper;
  std::unique_ptr<SceneManager> sceneMgr;

  etna::Image mainViewDepth;
  etna::Buffer constants;
  etna::Buffer zeroLengthBuffer;  // Bind it to unbind some other buffer.
  etna::Buffer relemToTextureMap;
  etna::Sampler defaultSampler;
  etna::PersistentDescriptorSet texturesDescriptorSet;
  etna::DescriptorSet relemToTextureMapDescriptorSet;
  std::vector<etna::Binding> bindings;
  std::vector<int> relemToTextureMapCPU;
  bool texturesDirty;

  PlaceholderTextureManager placeholderTextureManager;
  
  std::vector<vk::DrawIndexedIndirectCommand> drawCommands;
  etna::Buffer drawCommandsBuffer;

  struct PushConstants {
    glm::mat4x4 projView;
  } pushConsts;

  struct DrawParams
  {
    glm::mat4x4 model;
    int32_t relemIdx;
    int32_t padding[3];
  };

  etna::DescriptorSet drawParamsDescriptorSet;
  std::vector<DrawParams> drawParams;
  etna::Buffer drawParamsBuffer;

  glm::mat4x4 worldViewProj;
  glm::mat4x4 lightMatrix;

  etna::GraphicsPipeline staticMeshPipeline{};

  glm::uvec2 resolution;
};
