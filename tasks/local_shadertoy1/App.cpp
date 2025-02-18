#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <iostream>


App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();

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

  {
    etna::create_program("shadertoy1", {LOCAL_SHADERTOY1_SHADERS_ROOT "toy.comp.spv"});

    computePipeline = etna::get_context().getPipelineManager().createComputePipeline("shadertoy1", {});

    mainImage = etna::get_context().createImage(etna::Image::CreateInfo{
      .extent = vk::Extent3D{resolution.x, resolution.y, 1},
      .name = "resultImage",
      .format = vk::Format::eR8G8B8A8Unorm,
      .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
    });

    sampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
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

  etna::begin_frame();

  auto nextSwapchainImage = vkWindow->acquireNext();

  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);

      auto simpleComputeInfo = etna::get_shader_program("shadertoy1");

      auto set = etna::create_descriptor_set(
        simpleComputeInfo.getDescriptorLayoutId(0),
        currentCmdBuf,
        {
          etna::Binding{0, mainImage.genBinding(sampler.get(), vk::ImageLayout::eGeneral)},
        });

      vk::DescriptorSet vkSet = set.getVkSet();

      currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline.getVkPipeline());
      currentCmdBuf.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, computePipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, nullptr);

      {
        float time = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count() - startTime) / 1000.0f;
        currentCmdBuf.pushConstants(
            computePipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute,
            0, sizeof(time), &time);
      }

      etna::flush_barriers(currentCmdBuf);
      currentCmdBuf.dispatch(resolution.x / 32, resolution.y / 16, 1);

      etna::set_state(
        currentCmdBuf,
        mainImage.get(),
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      auto subres = vk::ImageSubresourceLayers( vk::ImageAspectFlagBits::eColor, 0, 0, 1 );
      vk::ArrayWrapper1D<vk::Offset3D,2> offsets = {{ vk::Offset3D( 0, 0, 0 ), vk::Offset3D( resolution.x, resolution.y, 1 ) }};
      vk::ImageBlit toCopy = {subres, offsets, subres, offsets};
      currentCmdBuf.blitImage(
        mainImage.get(),
        vk::ImageLayout::eTransferSrcOptimal,
        backbuffer,
        vk::ImageLayout::eTransferDstOptimal,
        1,
        &toCopy,
        vk::Filter::eLinear
      );

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
