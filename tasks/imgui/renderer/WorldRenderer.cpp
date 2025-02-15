#include "WorldRenderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <glm/ext.hpp>
#include "imgui.h"
#include "stb_image.h"

#include <math.h>
#ifndef M_PIf
#define M_PIf 3.14159265358979323846f
#endif

WorldRenderer::WorldRenderer()
  : sceneMgr{std::make_unique<SceneManager>()}
{
  renderContext.sceneMgr = &*sceneMgr;
}

void WorldRenderer::allocateResources(glm::uvec2 swapchain_resolution)
{
  resolution = swapchain_resolution;

  // auto& ctx = etna::get_context();

  backbuffer2.allocate(resolution);

  staticMeshPipeline2.allocate();
  terrainPipeline2   .allocate();
  skyboxPipeline2    .allocate();
  resolveGPipeline2  .allocate();
  tonemapPipeline2   .allocate();

  defaultSampler = etna::Sampler({
      .filter = vk::Filter::eLinear,
      .name = "perlinSample",
  });

  // regenTerrain();
  
  gbuffer2.allocate(resolution);
}


void WorldRenderer::loadScene(std::filesystem::path path)
{
  if (path.stem().string().ends_with("_baked"))
  {
    sceneMgr->selectSceneBaked(path);
  }
  else
  {
    sceneMgr->selectScene(path);
  }
  staticMeshPipeline2.reserve(sceneMgr->getRenderElements().size());
}

void WorldRenderer::loadShaders()
{
  staticMeshPipeline2.loadShaders();

  terrainPipeline2.loadShaders();

  tonemapPipeline2.loadShaders();

  skyboxPipeline2.loadShaders();

  resolveGPipeline2.loadShaders();

  // etna::create_program("static_mesh", {IMGUI_RENDERER_SHADERS_ROOT "static_mesh.vert.spv"});
  spdlog::info("Shaders loaded");
}

void WorldRenderer::setupPipelines(vk::Format /*swapchain_format*/)
{
  staticMeshPipeline2.setup();
  terrainPipeline2   .setup();
  skyboxPipeline2    .setup();
  resolveGPipeline2  .setup();
  tonemapPipeline2   .setup();
}

void WorldRenderer::debugInput(const Keyboard& kb) 
{

  staticMeshPipeline2.debugInput(kb);
  terrainPipeline2   .debugInput(kb);
  skyboxPipeline2    .debugInput(kb);
  resolveGPipeline2  .debugInput(kb);
  tonemapPipeline2   .debugInput(kb);

  if (kb[KeyboardKey::kPause] == ButtonState::Falling)
  {
    pause = !pause;
    spdlog::info("Pause is {}", pause ? "on" : "off");
  }
 
}

void WorldRenderer::update(const FramePacket& packet)
{
  ZoneScoped;

  // calc camera matrix
  {
    const float aspect = float(resolution.x) / float(resolution.y);
    renderContext.worldViewProj = packet.mainCam.projTm(aspect) * packet.mainCam.viewTm();
    renderContext.worldView = packet.mainCam.viewTm();
    renderContext.worldProj = packet.mainCam.projTm(aspect);
    renderContext.camPos = packet.mainCam.position;
  }

  if(!pause) {
    renderContext.frameTime = packet.currentTime; 
  }
  
}

void WorldRenderer::renderWorld(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderToGBuffer);
  {
    
    for(std::size_t i = 0; i < targets::GBuffer::N_COLOR_ATTACHMENTS; i++) {
      etna::set_state(cmd_buf, 
        gbuffer2.getImage(i).get(), 
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
        vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eColorAttachmentOptimal, 
        vk::ImageAspectFlagBits::eColor
      );
    }
    etna::set_state(cmd_buf, 
        gbuffer2.getDepthImage().get(), 
        vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
        vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eDepthAttachmentOptimal, 
        vk::ImageAspectFlagBits::eDepth
      );
    etna::flush_barriers(cmd_buf);
    
    etna::RenderTargetState renderTargets({
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      gbuffer2.getColorAttachments(),
      gbuffer2.getDepthAttachment(),
      {}
    });

    terrainPipeline2.render(cmd_buf, gbuffer2, renderContext);
    staticMeshPipeline2.render(cmd_buf, gbuffer2, renderContext);
  }

  {
    ETNA_PROFILE_GPU(cmd_buf, renderToBackbuffer);
    etna::set_state(cmd_buf, 
      backbuffer2.get(), 
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, 
      {}, 
      vk::ImageLayout::eColorAttachmentOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
  
    for(std::size_t i = 0; i < targets::GBuffer::N_COLOR_ATTACHMENTS; i++) {
      etna::set_state(cmd_buf, 
        gbuffer2.getImage(i).get(), 
        vk::PipelineStageFlagBits2::eFragmentShader, 
        vk::AccessFlagBits2::eShaderSampledRead, 
        vk::ImageLayout::eShaderReadOnlyOptimal, 
        vk::ImageAspectFlagBits::eColor
      );
    }
    etna::set_state(cmd_buf, 
      gbuffer2.getDepthImage().get(), 
      vk::PipelineStageFlagBits2::eFragmentShader, 
      vk::AccessFlagBits2::eShaderSampledRead, 
      vk::ImageLayout::eShaderReadOnlyOptimal, 
      vk::ImageAspectFlagBits::eDepth
    );
    
    etna::flush_barriers(cmd_buf);
    etna::RenderTargetState renderTargets({
      cmd_buf,
      {{0, 0}, {resolution.x, resolution.y}},
      backbuffer2.getColorAttachments(),
      backbuffer2.getDepthAttachment(),
      {}
    });
    
    skyboxPipeline2.render(cmd_buf, backbuffer2, renderContext);
    resolveGPipeline2.render(cmd_buf, gbuffer2, renderContext, skyboxPipeline2.getImage());
  }

  
  renderPostprocess(cmd_buf, target_image, target_image_view);
}

void WorldRenderer::renderPostprocess(
  vk::CommandBuffer cmd_buf, vk::Image target_image, vk::ImageView target_image_view)
{
  ETNA_PROFILE_GPU(cmd_buf, renderWorld);
  if(!tonemapPipeline2.enabled())
  {
    etna::set_state(cmd_buf, 
      target_image, 
      vk::PipelineStageFlagBits2::eTransfer, 
      vk::AccessFlagBits2::eTransferWrite, 
      vk::ImageLayout::eTransferDstOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
    etna::set_state(cmd_buf, 
      backbuffer2.get(), 
      vk::PipelineStageFlagBits2::eTransfer, 
      vk::AccessFlagBits2::eTransferRead, 
      vk::ImageLayout::eTransferSrcOptimal, 
      vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd_buf);
    std::array<vk::Offset3D, 2> offs = {
        vk::Offset3D{},
        vk::Offset3D{.x =static_cast<int32_t>(resolution.x), .y=static_cast<int32_t>(resolution.y), .z = 1}
    };
    std::array<vk::ImageBlit, 1> blit = {
      vk::ImageBlit{
          .srcSubresource = {.aspectMask=vk::ImageAspectFlagBits::eColor, .layerCount=1,},
          .srcOffsets = offs,
          .dstSubresource = {.aspectMask=vk::ImageAspectFlagBits::eColor, .layerCount=1},
          .dstOffsets = offs,
        }
    };
    cmd_buf.blitImage(backbuffer2.get(), vk::ImageLayout::eTransferSrcOptimal,
      target_image, vk::ImageLayout::eTransferDstOptimal,
      blit,
      vk::Filter::eLinear
    );
    etna::set_state(cmd_buf, 
      target_image, 
      vk::PipelineStageFlagBits2::eAllCommands, 
      {}, 
      vk::ImageLayout::ePresentSrcKHR, 
      vk::ImageAspectFlagBits::eColor
    );
    etna::flush_barriers(cmd_buf);
    return;
  }

  etna::set_state(cmd_buf, 
    backbuffer2.get(), 
    vk::PipelineStageFlagBits2::eComputeShader, 
    vk::AccessFlagBits2::eShaderRead, 
    vk::ImageLayout::eGeneral, 
    vk::ImageAspectFlagBits::eColor
  );
  etna::flush_barriers(cmd_buf);

  tonemapPipeline2.tonemapEvaluate(cmd_buf, backbuffer2, renderContext);
  
  etna::set_state(cmd_buf, 
    backbuffer2.get(), 
    vk::PipelineStageFlagBits2::eFragmentShader, 
    vk::AccessFlagBits2::eShaderSampledRead, 
    vk::ImageLayout::eShaderReadOnlyOptimal, 
    vk::ImageAspectFlagBits::eColor
  );
  etna::flush_barriers(cmd_buf);

  etna::RenderTargetState renderTargets(
    cmd_buf,
    {{0, 0}, {resolution.x, resolution.y}},
    {{.image = target_image, .view = target_image_view}},
    {}
  );

  tonemapPipeline2.render(cmd_buf, backbuffer2, renderContext);
}




void 
WorldRenderer::drawGui()
{
  ImGui::Begin("Render settings");
  {
    
    if(ImGui::TreeNode("Terrain settings"))
    {
      terrainPipeline2.drawGui();
      ImGui::TreePop();
    }
    if(ImGui::TreeNode("StaticMesh renderer settings"))
    {
      staticMeshPipeline2.drawGui();
      ImGui::TreePop();
    }
    
    if(ImGui::TreeNode("tonemap")) {
      tonemapPipeline2.drawGui();
      ImGui::TreePop();
    }
  }
  ImGui::End();
}