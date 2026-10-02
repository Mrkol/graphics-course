#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>


App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.
  {
    // GLFW tells us which extensions it needs to present frames to the OS window.
    // Actually rendering anything to a screen is optional in Vulkan, you can
    // alternatively save rendered frames into files, send them over network, etc.
    // Instance extensions do not depend on the actual GPU, only on the OS.
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    // We also need the swapchain device extension to get access to the OS
    // window from inside of Vulkan on the GPU.
    // Device extensions require HW support from the GPU.
    // Generally, in Vulkan, we call the GPU a "device" and the CPU/OS combination a "host."
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Etna does all of the Vulkan initialization heavy lifting.
    // You can skip figuring out how it works for now.
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
  }

  // Next, we need a magical Etna helper to send commands to the GPU.
  // How it is actually performed is not trivial, but we can skip this for now.
  commandManager = etna::get_context().createPerFrameCmdMgr();

  // Now we can create an OS window
  osWindow = windowing.createWindow(
    OsWindow::CreateInfo{
      .resolution = resolution,
    });

  // But we also need to hook the OS window up to Vulkan manually!
  {
    // First, we ask GLFW to provide a "surface" for the window,
    // which is an opaque description of the area where we can actually render.
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    // Then we pass it to Etna to do the complicated work for us
    vkWindow = etna::get_context().createWindow(
      etna::Window::CreateInfo{
        .surface = std::move(surface),
      });

    // And finally ask Etna to create the actual swapchain so that we can
    // get (different) images each frame to render stuff into.
    // Here, we do not support window resizing, so we only need to call this once.
    auto [w, h] = vkWindow->recreateSwapchain(
      etna::Window::DesiredProperties{
        .resolution = {resolution.x, resolution.y},
        .vsync = useVsync,
        .numFramesInFlight = static_cast<uint32_t>(commandManager->getCmdBufferCount()),
      });

    // Technically, Vulkan might fail to initialize a swapchain with the requested
    // resolution and pick a different one. This, however, does not occur on platforms
    // we support. Still, it's better to follow the "intended" path.
    resolution = {w, h};
  }

  // TODO: Initialize any additional resources you require here!
  image = etna::get_context().createImage(
    {.extent = vk::Extent3D{resolution.x, resolution.y, 1u},
     .name = "output_image",
     .format = vk::Format::eR8G8B8A8Unorm,
     .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc});

  etna::create_program("local_shadertoy", {LOCAL_SHADERTOY1_SHADERS_ROOT "toy.comp.spv"});
  computePipeline = etna::get_context().getPipelineManager().createComputePipeline("local_shadertoy", {});
  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  startTime = std::chrono::steady_clock::now();

  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::buildComputeCommands(vk::CommandBuffer current_cmd_buf)
{
  etna::set_state(
    current_cmd_buf,
    image.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderStorageWrite,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);

  auto computeInfo = etna::get_shader_program("local_shadertoy");
  auto set = etna::create_descriptor_set(
    computeInfo.getDescriptorLayoutId(0), current_cmd_buf, {etna::Binding{0, image.genBinding(defaultSampler.get(), vk::ImageLayout::eGeneral)}});

  current_cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline.getVkPipeline());
  current_cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, computePipeline.getVkPipelineLayout(), 0, {set.getVkSet()}, {});

  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<float> elapsed = now - startTime;
  float time = elapsed.count();
  current_cmd_buf.pushConstants(computePipeline.getVkPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(time), &time);

  etna::flush_barriers(current_cmd_buf);

  current_cmd_buf.dispatch(resolution.x / 32, resolution.y / 32, 1);
}

vk::ImageCopy App::buildFullImageCopy()
{
  vk::ImageCopy copyRegion{
    .srcSubresource = vk::ImageSubresourceLayers{.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
    .srcOffset = vk::Offset3D{0, 0, 0},
    .dstSubresource = vk::ImageSubresourceLayers{.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
    .dstOffset = vk::Offset3D{0, 0, 0},
    .extent = vk::Extent3D{resolution.x, resolution.y, 1}};
  return copyRegion;
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
    auto [backbuffer, backbufferView, backbufferAvailableSem, backbufferReadyForPresentSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      // First of all, we need to "initialize" th "backbuffer", aka the current swapchain
      // image, into a state that is appropriate for us working with it. The initial state
      // is considered to be "undefined" (aka "I contain trash memory"), by the way.
      // "Transfer" in vulkanese means "copy or blit".
      // Note that Etna sometimes calls this for you to make life simpler, read Etna's code!
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        // We are going to use the texture at the transfer stage...
        vk::PipelineStageFlagBits2::eTransfer,
        // ...to transfer-write stuff into it...
        vk::AccessFlagBits2::eTransferWrite,
        // ...and want it to have the appropriate layout.
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);
      // The set_state doesn't actually record any commands, they are deferred to
      // the moment you call flush_barriers.
      // As with set_state, Etna sometimes flushes on it's own.
      // Usually, flushes should be placed before "action", i.e. compute dispatches
      // and blit/copy operations.
      etna::flush_barriers(currentCmdBuf);

      buildComputeCommands(currentCmdBuf);
      etna::flush_barriers(currentCmdBuf);

      vk::ImageCopy copyRegion = buildFullImageCopy();

      currentCmdBuf.copyImage(image.get(), vk::ImageLayout::eGeneral, backbuffer, vk::ImageLayout::eTransferDstOptimal, copyRegion);
      etna::flush_barriers(currentCmdBuf);

      // At the end of "rendering", we are required to change how the pixels of the
      // swapchain image are laid out in memory to something that is appropriate
      // for presenting to the window (while preserving the content of the pixels!).
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        // This looks weird, but is correct. Ask about it later.
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      // And of course flush the layout transition.
      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    // Note that the GPU won't start executing our commands before the backbufferAvailableSem
    // semaphore is signalled, which will happen when the OS says that the next swapchain image
    // is ready, and the result image will be ready for present after backbufferReadyForPresent
    // is signalled by GPU
    auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem), std::move(backbufferReadyForPresentSem));

    // Finally, present the backbuffer the screen, but only after the GPU tells the OS
    // that it is done executing the command buffer via the renderingDone semaphore.
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
