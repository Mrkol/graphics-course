#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <etna/BlockingTransferHelper.hpp>
#include <iostream>
#include <ctime>


App::App()
  : resolution{1280, 720}
  , skinTextureResolution{128, 128}
  , useVsync{true}
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    resolution = {w, h};
  }

  commandManager = etna::get_context().createPerFrameCmdMgr();
  oneShotManager = etna::get_context().createOneShotCmdMgr();

  {
    etna::create_program("shadertoy2", {LOCAL_SHADERTOY2_SHADERS_ROOT "toy.vert.spv",
                                        LOCAL_SHADERTOY2_SHADERS_ROOT "toy.frag.spv" });
    etna::create_program("skinTexture", {LOCAL_SHADERTOY2_SHADERS_ROOT "toy.vert.spv",
                                        LOCAL_SHADERTOY2_SHADERS_ROOT "texture.frag.spv" });

    
    fragVertPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline("shadertoy2",
      etna::GraphicsPipeline::CreateInfo {
        .fragmentShaderOutput = {
          .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}
        }
      });
    
    skinTexturePipeline = etna::get_context().getPipelineManager().createGraphicsPipeline("skinTexture",
      etna::GraphicsPipeline::CreateInfo {
        .fragmentShaderOutput = {
          .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}
        }
      });

    skinTextureImage = etna::get_context().createImage(etna::Image::CreateInfo{
      .extent = vk::Extent3D{skinTextureResolution.x, skinTextureResolution.y, 1},
      .name = "skinTexture",
      .format = vk::Format::eB8G8R8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    });

    defaultSampler = etna::Sampler(etna::Sampler::CreateInfo
      {
        .filter = vk::Filter::eLinear,
        .addressMode = vk::SamplerAddressMode::eRepeat,
        .name = "default_sampler"
      });
  }
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame()
{
  auto currentCmdBuf = commandManager->acquireNext();

  if (!initializedFileTexture) {
    int x, y, n;
    unsigned char *picData = stbi_load(TEXTURES_ROOT "photo_2024-12-02_16-41-15.jpg", &x, &y, &n, 4);
    if (picData == NULL) {
      std::cerr << "photo_2024-12-02_16-41-15.jpg not found" << std::endl;
      std::terminate();
    }

    etna::Image::CreateInfo fileTextureInfo{
      .extent = vk::Extent3D{static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1},
      .name = "fileTexture",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
    };
    fileTextureImage = etna::create_image_from_bytes(fileTextureInfo, currentCmdBuf, picData);

    stbi_image_free(picData);
    initializedFileTexture = true;
  }

  etna::begin_frame();

  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      etna::set_state(
        currentCmdBuf,
        skinTextureImage.get(),
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor);
      
      etna::flush_barriers(currentCmdBuf);

      {
        etna::RenderTargetState state{currentCmdBuf, {{}, {skinTextureResolution.x, skinTextureResolution.y}},
          {{skinTextureImage.get(), skinTextureImage.getView({})}}, {}};
        auto skinInfo = etna::get_shader_program("skinTexture");

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, skinTexturePipeline.getVkPipeline());

        glm::uvec2 res = skinTextureResolution;
        currentCmdBuf.pushConstants(
          skinTexturePipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment,
          0, sizeof(res), &res);

        currentCmdBuf.draw(3, 1, 0, 0);
      }


      etna::set_state(
        currentCmdBuf,
        skinTextureImage.get(),
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor);
      
      etna::set_state(
        currentCmdBuf,
        fileTextureImage.get(),
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      {
        etna::RenderTargetState state{currentCmdBuf, {{}, {resolution.x, resolution.y}},
          {{backbuffer, backbufferView}}, {}};

        auto fragVertInfo = etna::get_shader_program("shadertoy2");
        auto set = etna::create_descriptor_set(
          fragVertInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {
            etna::Binding{0, skinTextureImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{1, fileTextureImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
          });

        vk::DescriptorSet vkSet = set.getVkSet();

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, fragVertPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, fragVertPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

        glm::uvec2 res = resolution;

        currentCmdBuf.pushConstants(
          fragVertPipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eFragment,
          0, sizeof(res), &res);

        currentCmdBuf.draw(3, 1, 0, 0);
      }

      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}