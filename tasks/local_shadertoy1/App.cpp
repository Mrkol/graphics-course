#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan.hpp>

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.

  // GLFW tells us which extensions it needs to present frames to the OS window.
  auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

  std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

  // We also need the swapchain device extension to get access to the OS
  // window from inside of Vulkan on the GPU.
  std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  // Etna does all of the Vulkan initialization heavy lifting.
  etna::initialize(
    etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });

  // Next, we need a magical Etna helper to send commands to the GPU.
  commandManager = etna::get_context().createPerFrameCmdMgr();

  // Now we can create an OS window
  osWindow = windowing.createWindow(
    OsWindow::CreateInfo{
      .resolution = resolution,
    });

  // But we also need to hook the OS window up to Vulkan manually!
  auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

  // Then we pass it to Etna to do the complicated work for us
  vkWindow = etna::get_context().createWindow(
    etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

  // And finally ask Etna to create the actual swapchain so that we can
  // get (different) images each frame to render stuff into.
  auto [w, h] = vkWindow->recreateSwapchain(
    etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
      .numFramesInFlight = static_cast<uint32_t>(commandManager->getCmdBufferCount()),
    });

  resolution = {w, h};

  etna::create_program("toy_shader", {LOCAL_SHADERTOY1_SHADERS_ROOT "toy.comp.spv"});
  pipeline = etna::get_context().getPipelineManager().createComputePipeline("toy_shader", {});
  image = etna::get_context().createImage(
    etna::Image::CreateInfo{
      .extent = vk::Extent3D{resolution.x, resolution.y, 1},
      .name = "toy_result",
      .format = vk::Format::eR8G8B8A8Unorm,
      .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc,
    });
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

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame()
{
  // First, get a command buffer to write GPU commands into.
  auto currentCmdBuf = commandManager->acquireNext();

  // Next, tell Etna that we are going to start processing the next frame.
  etna::begin_frame();

  // And now get the image we should be rendering the picture into.
  auto nextSwapchainImage = vkWindow->acquireNext();

  // When window is minimized, we can't render anything in Windows
  // because it kills the swapchain, so we skip frames in this case.
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem, backbufferReadyForPresentSem] =
      *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      // First of all, we need to "initialize" the "backbuffer", aka the current
      // swapchain image, into a state that is appropriate for us working with it.
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);

      etna::set_state(
        currentCmdBuf,
        image.get(),
        vk::PipelineStageFlagBits2::eComputeShader,
        vk::AccessFlagBits2::eShaderWrite,
        vk::ImageLayout::eGeneral,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      {
        auto toyInfo = etna::get_shader_program("toy_shader");

        auto set = etna::create_descriptor_set(
          toyInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {etna::Binding{0, image.genBinding({}, vk::ImageLayout::eGeneral)}});

        vk::DescriptorSet vkSet = set.getVkSet();

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eCompute, pipeline.getVkPipelineLayout(), 0, {vkSet}, {});

        constexpr uint32_t groupSize = 32;
        const uint32_t groupsX = (resolution.x + groupSize - 1) / groupSize;
        const uint32_t groupsY = (resolution.y + groupSize - 1) / groupSize;

        currentCmdBuf.dispatch(groupsX, groupsY, 1);
      }

      etna::set_state(
        currentCmdBuf,
        image.get(),
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      const vk::ImageSubresourceLayers subresource{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
      };
      const std::array<vk::Offset3D, 2> fullRegion{
        vk::Offset3D{0, 0, 0},
        vk::Offset3D{static_cast<int32_t>(resolution.x), static_cast<int32_t>(resolution.y), 1},
      };

      currentCmdBuf.blitImage(
        image.get(),
        vk::ImageLayout::eTransferSrcOptimal,
        backbuffer,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit{
          .srcSubresource = subresource,
          .srcOffsets = fullRegion,
          .dstSubresource = subresource,
          .dstOffsets = fullRegion,
        },
        vk::Filter::eLinear);
        
      // At the end of "rendering", we are required to change how the pixels of the
      // swapchain image are laid out in memory to something that is appropriate
      // for presenting to the window.
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

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    auto renderingDone = commandManager->submit(
      std::move(currentCmdBuf),
      std::move(backbufferAvailableSem),
      std::move(backbufferReadyForPresentSem));

    // Finally, present the backbuffer the screen.
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // After a window us un-minimized, we need to restore the swapchain to continue rendering.
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(
      etna::Window::DesiredProperties{
        .resolution = {resolution.x, resolution.y},
        .vsync = useVsync,
        .numFramesInFlight = static_cast<uint32_t>(commandManager->getCmdBufferCount()),
      });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
