#include "StaticMesh.hpp"

#include <etna/BlockingTransferHelper.hpp>
#include <etna/Etna.hpp>
#include <etna/Profiling.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>

#include "scene/SceneManager.hpp"

#include <imgui.h>
#include <glm/glm.hpp>

#include "stb_image.h"

namespace pipes {

void 
StaticMeshPipeline::allocate()
{
    auto& ctx = etna::get_context();
    instanceMatricesBuf = ctx.createBuffer({
      .size = N_MAX_INSTANCES * sizeof(glm::mat4x4),
      .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      .name = "instanceMatrices",
    });
    instanceMatricesBuf.map();

    defaultSampler = etna::Sampler({
      .filter = vk::Filter::eLinear,
      .name = "staticMeshSampler",
    });
}

void 
StaticMeshPipeline::loadShaders() 
{
    etna::create_program(
        "staticmesh_shader",
        { STATICMESH_PIPELINE_SHADERS_ROOT "staticmesh.vert.spv",
          STATICMESH_PIPELINE_SHADERS_ROOT "staticmesh.frag.spv"}
    );
}

void 
StaticMeshPipeline::setup() 
{
  auto& pipelineManager = etna::get_context().getPipelineManager();

  etna::VertexShaderInputDescription sceneVertexInputDesc{
    .bindings = {etna::VertexShaderInputDescription::Binding{
      .byteStreamDescription = SceneManager::getVertexFormatDescription(),
    }},
  };

  pipeline = pipelineManager.createGraphicsPipeline(
    "staticmesh_shader",
    etna::GraphicsPipeline::CreateInfo{
      .vertexShaderInput = sceneVertexInputDesc,
      .rasterizationConfig =
        vk::PipelineRasterizationStateCreateInfo{
          .polygonMode = vk::PolygonMode::eFill,
          .cullMode = vk::CullModeFlagBits::eBack,
          .frontFace = vk::FrontFace::eCounterClockwise,
          .lineWidth = 1.f,
        },
      .blendingConfig = {
        .attachments = {
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
          vk::PipelineColorBlendAttachmentState{
            .blendEnable = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
          },
        },
        .logicOpEnable = false,
        .logicOp = {},
        .blendConstants = {},
      },
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = RenderTarget::COLOR_ATTACHMENT_FORMATS,
          .depthAttachmentFormat = RenderTarget::DEPTH_ATTACHMENT_FORMAT,
        },
    });
}

void 
StaticMeshPipeline::drawGui()
{
  ImGui::Checkbox("culling", &enableCulling);
  ImGui::Checkbox("use normal maps", &normalMap);
}

void 
StaticMeshPipeline::debugInput(const Keyboard& kb)
{
  if (kb[KeyboardKey::kN] == ButtonState::Falling) {
    normalMap = !normalMap;
    spdlog::info("normal maps are {}", normalMap ? "on" : "off");
  }
}

targets::GBuffer& 
StaticMeshPipeline::render(vk::CommandBuffer cmd_buf, targets::GBuffer& target, const RenderContext& ctx)
{
  
  // etna::RenderTargetState renderTargets(
  //   cmd_buf,
  //   {{0, 0}, {ctx.resolution.x, ctx.resolution.y}},
  //   gBufferColorAttachments,
  //   gBufferDepthAttachment
  // );
    

  cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.getVkPipeline());
  if (!ctx.sceneMgr->getVertexBuffer())
    return target;

  prepareFrame(ctx);

  auto staticMesh = etna::get_shader_program("staticmesh_shader");

  cmd_buf.bindVertexBuffers(0, {ctx.sceneMgr->getVertexBuffer()}, {0});
  cmd_buf.bindIndexBuffer(ctx.sceneMgr->getIndexBuffer(), 0, vk::IndexType::eUint32);


  pushConst2M.projView = ctx.worldViewProj;
  auto set0 = etna::create_descriptor_set(
  staticMesh.getDescriptorLayoutId(0),
  cmd_buf,
  {
    etna::Binding{0, instanceMatricesBuf.genBinding()},
  });
  auto relems = ctx.sceneMgr->getRenderElements();
  std::size_t firstInstance = 0;
  for (std::size_t j = 0; j < relems.size(); ++j)
  {
    //Skip drawing water as it is rendered by terrain
    if (j == 8) {
      firstInstance += nInstances[j];
      continue;
    }  
    if (nInstances[j] != 0)
    {
      Material::Id mid = relems[j].materialId;
      auto& material = ctx.sceneMgr->get(mid == Material::Id::Invalid ? ctx.sceneMgr->getStubMaterial() : relems[j].materialId);
      auto& baseColorImage = ctx.sceneMgr->get(material.baseColorTexture).image;
      auto& normalImage = normalMap ? ctx.sceneMgr->get(material.normalTexture).image : ctx.sceneMgr->get(ctx.sceneMgr->getStubBlueTexture()).image;
      auto& metallicRoughnessImage = normalMap ? ctx.sceneMgr->get(material.metallicRoughnessTexture).image : ctx.sceneMgr->get(ctx.sceneMgr->getStubTexture()).image;
      auto set1 = etna::create_descriptor_set(
        staticMesh.getDescriptorLayoutId(1),
        cmd_buf,
        {
          etna::Binding{0, baseColorImage        .genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding{1, normalImage           .genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
          etna::Binding{2, metallicRoughnessImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
        });
      pushConst2M.color = material.baseColor;
      pushConst2M.emr_factors = material.EMR_Factor;
      cmd_buf.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        pipeline.getVkPipelineLayout(),
        0,
        {set0.getVkSet(), set1.getVkSet()},
        {});

      const auto& relem = relems[j];
      cmd_buf.pushConstants<PushConstants>(
        pipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, {pushConst2M});
      cmd_buf.drawIndexed(
        relem.indexCount, nInstances[j], relem.indexOffset, relem.vertexOffset, firstInstance);
      firstInstance += nInstances[j];
    }
  }
    return target;
}

static bool 
IsNotVisble(const glm::mat2x3 /*bounds*/, const glm::mat4x4& /*transform*/)
{
  return false;
  #if 0
  const glm::vec3 origin = transform * glm::vec4(bounds[0], 1.f);
  glm::mat3 oldExtent = glm::mat3(
    bounds[1].x, 0, 0,
    0, bounds[1].y, 0,
    0, 0, bounds[1].z
  );
  const glm::vec3 extent = glm::abs(glm::mat3(transform) * oldExtent) * glm::vec3(1.f, 1.f, 1.f);
  
  if(origin.z + extent.z < 0) return true;

  glm::vec3 lc = glm::vec3(origin) + extent;
  glm::vec3 rc = glm::vec3(origin) - extent;
  return 
        lc.x < -lc.z || 
        lc.y < -lc.z || 
        rc.x >  lc.z || 
        rc.y >  lc.z;
  #endif
}

void 
StaticMeshPipeline::prepareFrame(const RenderContext& ctx)
{
  auto instanceMeshes = ctx.sceneMgr->getInstanceMeshes();
  auto instanceMatrices = ctx.sceneMgr->getInstanceMatrices();
  auto meshes = ctx.sceneMgr->getMeshes();
  auto bbs = ctx.sceneMgr->getBounds();

  auto* matrices = reinterpret_cast<glm::mat4x4*>(instanceMatricesBuf.data());

  // May be do more fast clean
  std::memset(nInstances.data(), 0, nInstances.size() * sizeof(nInstances[0]));

  for (std::size_t i = 0; i < instanceMatrices.size(); ++i)
  {
    const auto meshIdx = instanceMeshes[i];
    const auto& instanceMatrix = instanceMatrices[i];
    for (std::size_t j = 0; j < meshes[meshIdx].relemCount; j++)
    {
      const auto relemIdx = meshes[meshIdx].firstRelem + j;
      if (enableCulling && IsNotVisble(bbs[relemIdx], ctx.worldViewProj * instanceMatrix))
      {
        continue;
      }
      nInstances[relemIdx]++;
      *matrices++ = instanceMatrix;
    }
  }
}

} /* namespace pipes */
