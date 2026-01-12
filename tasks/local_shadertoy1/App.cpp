#include "App.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/DescriptorSet.hpp>

#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>

#include "wsi/ButtonState.hpp"
#include "wsi/MouseButton.hpp"

namespace
{
constexpr const char* K_SHADERTOY_PROGRAM_NAME = "local_shadertoy1_copy_compute";

struct PushConstants
{
  glm::vec2 iResolution;
  float iTime;
};

static_assert(sizeof(PushConstants) <= 128, "PushConstants is too big");
} // namespace


App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  std::cout << "=== App Constructor Start ===" << std::endl;
  std::cout << "1. Starting App constructor" << std::endl;
  
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.
  {
    std::cout << "2. Getting GLFW extensions" << std::endl;
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    std::cout << "3. Setting up device extensions" << std::endl;
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    std::cout << "4. Initializing etna" << std::endl;
    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
    });
  }

  std::cout << "5. Creating command manager" << std::endl;
  commandManager = etna::get_context().createPerFrameCmdMgr();

  std::cout << "6. Creating OS window with resolution: " << resolution.x << "x" << resolution.y << std::endl;
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });
  std::cout << "6.5 OS window created: " << (osWindow ? "yes" : "no") << std::endl;

  std::cout << "7. Creating Vulkan window surface" << std::endl;
  // But we also need to hook the OS window up to Vulkan manually!
  {
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());
    std::cout << "8. Surface created, creating Vulkan window" << std::endl;

    // According to Window.hpp, CreateInfo only needs surface
    etna::Window::CreateInfo windowInfo{};
    windowInfo.surface = std::move(surface);
    
    std::cout << "9. Calling createWindow" << std::endl;
    vkWindow = etna::get_context().createWindow(std::move(windowInfo));
    std::cout << "10. Vulkan window created successfully: " << (vkWindow ? "yes" : "no") << std::endl;
    
    // CRITICAL FIX: We must create the swapchain before using the window!
    std::cout << "10.5 Creating swapchain with resolution: " << resolution.x << "x" << resolution.y 
              << ", vsync: " << (useVsync ? "yes" : "no") << std::endl;
    
    etna::Window::DesiredProperties swapchainProps{};
    swapchainProps.resolution = vk::Extent2D{resolution.x, resolution.y};
    swapchainProps.vsync = useVsync;
    swapchainProps.autoGamma = true; // Default from header
    
    auto actualExtent = vkWindow->recreateSwapchain(swapchainProps);
    std::cout << "10.6 Swapchain created with actual resolution: " 
              << actualExtent.width << "x" << actualExtent.height << std::endl;
    
    // Update our resolution in case swapchain chose different size
    resolution = glm::uvec2{actualExtent.width, actualExtent.height};
  }

  std::cout << "11. Setting up timing" << std::endl;
  startTimeSeconds = windowing.getTime();
  lastFrameTimestamp = startTimeSeconds;
  std::cout << "11.5 Start time: " << startTimeSeconds << std::endl;

  std::cout << "12. Creating compute pipeline" << std::endl;
  createComputePipeline();
  
  std::cout << "13. Ensuring storage image" << std::endl;
  ensureStorageImage();
  
  std::cout << "14. App constructor completed successfully" << std::endl;
  std::cout << "=== App Constructor End ===" << std::endl;
}

App::~App()
{
  std::cout << "=== App Destructor Start ===" << std::endl;
  std::cout << "App destructor called" << std::endl;
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
  std::cout << "=== App Destructor End ===" << std::endl;
}

void App::run()
{
  std::cout << "\n=== Run Loop Start ===" << std::endl;
  std::cout << "Starting run loop" << std::endl;
  int frame_count = 0;
  
  while (!osWindow->isBeingClosed())
  {
    std::cout << "\n--- Frame " << frame_count++ << " ---" << std::endl;
    std::cout << "Polling events" << std::endl;
    windowing.poll();
    
    std::cout << "Calling drawFrame()" << std::endl;
    try {
      drawFrame();
    } catch (const std::exception& e) {
      std::cout << "EXCEPTION in drawFrame: " << e.what() << std::endl;
      break;
    } catch (...) {
      std::cout << "UNKNOWN EXCEPTION in drawFrame" << std::endl;
      break;
    }
  }
  std::cout << "=== Run Loop End ===" << std::endl;
}

void App::drawFrame()
{
  std::cout << "drawFrame() - Start" << std::endl;
  
  // First, get a command buffer to write GPU commands into.
  std::cout << "drawFrame() - Acquiring command buffer" << std::endl;
  auto currentCmdBuf = commandManager->acquireNext();
  std::cout << "drawFrame() - Command buffer acquired" << std::endl;

  // Next, tell Etna that we are going to start processing the next frame.
  std::cout << "drawFrame() - Beginning frame" << std::endl;
  etna::begin_frame();
  std::cout << "drawFrame() - Frame begun" << std::endl;

  // And now get the image we should be rendering the picture into.
  std::cout << "drawFrame() - Acquiring next swapchain image" << std::endl;
  auto nextSwapchainImage = vkWindow->acquireNext();
  std::cout << "drawFrame() - acquireNext() result: " << (nextSwapchainImage ? "SUCCESS" : "FAILED/EMPTY") << std::endl;

  // When window is minimized, we can't render anything in Windows
  // because it kills the swapchain, so we skip frames in this case.
  if (nextSwapchainImage)
  {
    std::cout << "drawFrame() - Processing swapchain image" << std::endl;
    
    // According to Window.hpp, SwapchainImage has:
    // vk::Image image;
    // vk::ImageView view;
    // vk::Semaphore available;
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;
    std::cout << "drawFrame() - Got backbuffer and semaphore" << std::endl;

    std::cout << "drawFrame() - Beginning command buffer recording" << std::endl;
    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    std::cout << "drawFrame() - Command buffer begin successful" << std::endl;
    
    {
      std::cout << "drawFrame() - Setting backbuffer state to TransferDstOptimal" << std::endl;
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageAspectFlagBits::eColor);
      
      std::cout << "drawFrame() - Flushing barriers" << std::endl;
      etna::flush_barriers(currentCmdBuf);

      std::cout << "drawFrame() - Recording shadertoy commands" << std::endl;
      recordShadertoy(currentCmdBuf, backbuffer);

      std::cout << "drawFrame() - Setting backbuffer state to PresentSrcKHR" << std::endl;
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      
      std::cout << "drawFrame() - Flushing final barriers" << std::endl;
      etna::flush_barriers(currentCmdBuf);
    }
    
    std::cout << "drawFrame() - Ending command buffer recording" << std::endl;
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());
    std::cout << "drawFrame() - Command buffer recording ended" << std::endl;

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    std::cout << "drawFrame() - Submitting command buffer" << std::endl;
    auto renderingDone = commandManager->submit(
      std::move(currentCmdBuf),
      std::move(backbufferAvailableSem));
    std::cout << "drawFrame() - Command buffer submitted" << std::endl;

    // Finally, present the backbuffer the screen
    std::cout << "drawFrame() - Presenting frame" << std::endl;
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);
    std::cout << "drawFrame() - Present result: " << (presented ? "SUCCESS" : "FAILED") << std::endl;

    if (!presented)
    {
      std::cout << "drawFrame() - Present failed, swapchain might be out of date" << std::endl;
      // If present fails, we might need to recreate swapchain
      // This can happen if window was resized
      auto currentRes = osWindow->getResolution();
      if (currentRes.x > 0 && currentRes.y > 0) {
        std::cout << "drawFrame() - Recreating swapchain with new resolution: " 
                  << currentRes.x << "x" << currentRes.y << std::endl;
        etna::Window::DesiredProperties newProps{};
        newProps.resolution = vk::Extent2D{currentRes.x, currentRes.y};
        newProps.vsync = useVsync;
        newProps.autoGamma = true;
        
        auto actualExtent = vkWindow->recreateSwapchain(newProps);
        resolution = glm::uvec2{actualExtent.width, actualExtent.height};
        std::cout << "drawFrame() - Swapchain recreated with resolution: " 
                  << resolution.x << "x" << resolution.y << std::endl;
      }
    }
  }
  else
  {
    std::cout << "drawFrame() - No swapchain image available (window minimized or swapchain outdated)" << std::endl;
    // If acquireNext returns nullopt, swapchain might be out of date
    // Check if we need to recreate it
    auto currentRes = osWindow->getResolution();
    if (currentRes.x > 0 && currentRes.y > 0 && currentRes != resolution) {
      std::cout << "drawFrame() - Window resized, recreating swapchain" << std::endl;
      etna::Window::DesiredProperties newProps{};
      newProps.resolution = vk::Extent2D{currentRes.x, currentRes.y};
      newProps.vsync = useVsync;
      newProps.autoGamma = true;
      
      auto actualExtent = vkWindow->recreateSwapchain(newProps);
      resolution = glm::uvec2{actualExtent.width, actualExtent.height};
      std::cout << "drawFrame() - Swapchain recreated with resolution: " 
                << resolution.x << "x" << resolution.y << std::endl;
    }
  }

  std::cout << "drawFrame() - Ending frame" << std::endl;
  etna::end_frame();
  std::cout << "drawFrame() - Frame ended" << std::endl;
  
  std::cout << "drawFrame() - End" << std::endl;
}

void App::recordShadertoy(vk::CommandBuffer cmd_buf, vk::Image backbuffer)
{
  std::cout << "recordShadertoy() - Start" << std::endl;
  
  // Safety check
  if (resolution.x == 0 || resolution.y == 0) {
    std::cout << "ERROR: Invalid resolution: " << resolution.x << "x" << resolution.y << std::endl;
    return;
  }
  
  std::cout << "recordShadertoy() - Ensuring storage image" << std::endl;
  ensureStorageImage();

  std::cout << "recordShadertoy() - Getting frame timing" << std::endl;
  const auto [elapsed, delta] = pullFrameTiming();
  std::cout << "recordShadertoy() - Elapsed time: " << elapsed << ", delta: " << delta << std::endl;

  std::cout << "recordShadertoy() - Setting storage image state to General" << std::endl;
  etna::set_state(
    cmd_buf,
    storageImage.get(),
    vk::PipelineStageFlagBits2::eComputeShader,
    vk::AccessFlagBits2::eShaderWrite,
    vk::ImageLayout::eGeneral,
    vk::ImageAspectFlagBits::eColor);
  std::cout << "recordShadertoy() - Flushing barriers" << std::endl;
  etna::flush_barriers(cmd_buf);

  std::cout << "recordShadertoy() - Getting shader program info" << std::endl;
  auto programInfo = etna::get_shader_program(K_SHADERTOY_PROGRAM_NAME);
  std::cout << "recordShadertoy() - Creating descriptor set" << std::endl;
  auto descriptorSet = etna::create_descriptor_set(
    programInfo.getDescriptorLayoutId(0),
    cmd_buf,
    {etna::Binding{0, storageImage.genBinding(vk::Sampler{}, vk::ImageLayout::eGeneral)}});

  vk::DescriptorSet vkSet = descriptorSet.getVkSet();

  std::cout << "recordShadertoy() - Binding compute pipeline" << std::endl;
  cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, shadertoyPipeline.getVkPipeline());
  std::cout << "recordShadertoy() - Binding descriptor sets" << std::endl;
  cmd_buf.bindDescriptorSets(
    vk::PipelineBindPoint::eCompute,
    shadertoyPipeline.getVkPipelineLayout(),
    0,
    1,
    &vkSet,
    0,
    nullptr);

  PushConstants constants{};
  constants.iResolution =
    glm::vec2(static_cast<float>(resolution.x), static_cast<float>(resolution.y));
  constants.iTime = elapsed;

  std::cout << "recordShadertoy() - Push constants - iResolution: " << constants.iResolution.x 
            << "x" << constants.iResolution.y << ", iTime: " << constants.iTime << std::endl;

  std::cout << "recordShadertoy() - Pushing constants" << std::endl;
  cmd_buf.pushConstants(
    shadertoyPipeline.getVkPipelineLayout(),
    vk::ShaderStageFlagBits::eCompute,
    0,
    sizeof(PushConstants),
    &constants);

  std::cout << "recordShadertoy() - Flushing barriers before dispatch" << std::endl;
  etna::flush_barriers(cmd_buf);

  std::cout << "recordShadertoy() - Calculating dispatch groups" << std::endl;
  const auto dispatchGroups = calcDispatchGroupCount();
  std::cout << "recordShadertoy() - Dispatching compute: " << dispatchGroups.x 
            << "x" << dispatchGroups.y << " groups" << std::endl;
  cmd_buf.dispatch(dispatchGroups.x, dispatchGroups.y, 1);

  std::cout << "recordShadertoy() - Setting storage image state to TransferSrcOptimal" << std::endl;
  etna::set_state(
    cmd_buf,
    storageImage.get(),
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlagBits2::eTransferRead,
    vk::ImageLayout::eTransferSrcOptimal,
    vk::ImageAspectFlagBits::eColor);
  std::cout << "recordShadertoy() - Flushing barriers for blit" << std::endl;
  etna::flush_barriers(cmd_buf);

  const vk::ImageSubresourceLayers subresource{
    .aspectMask = vk::ImageAspectFlagBits::eColor,
    .mipLevel = 0,
    .baseArrayLayer = 0,
    .layerCount = 1,
  };

  const vk::Offset3D srcExtent{
    static_cast<int32_t>(storageImageExtent.x),
    static_cast<int32_t>(storageImageExtent.y),
    1,
  };
  const vk::Offset3D dstExtent{
    static_cast<int32_t>(resolution.x),
    static_cast<int32_t>(resolution.y),
    1,
  };

  const vk::ImageBlit region{
    .srcSubresource = subresource,
    .srcOffsets = std::array<vk::Offset3D, 2>{vk::Offset3D{0, 0, 0}, srcExtent},
    .dstSubresource = subresource,
    .dstOffsets = std::array<vk::Offset3D, 2>{vk::Offset3D{0, 0, 0}, dstExtent},
  };

  std::cout << "recordShadertoy() - Blitting image from storage to backbuffer" << std::endl;
  cmd_buf.blitImage(
    storageImage.get(),
    vk::ImageLayout::eTransferSrcOptimal,
    backbuffer,
    vk::ImageLayout::eTransferDstOptimal,
    {region},
    vk::Filter::eLinear);
    
  std::cout << "recordShadertoy() - End" << std::endl;
}

void App::createComputePipeline()
{
  std::cout << "createComputePipeline() - Start" << std::endl;
  std::cout << "Creating compute pipeline" << std::endl;
  etna::create_program(K_SHADERTOY_PROGRAM_NAME, {"tasks/local_shadertoy1/shaders/toy.comp.spv"});

  auto& pipelineManager = etna::get_context().getPipelineManager();
  shadertoyPipeline = pipelineManager.createComputePipeline(K_SHADERTOY_PROGRAM_NAME, {});
  std::cout << "Compute pipeline created" << std::endl;
  std::cout << "createComputePipeline() - End" << std::endl;
}

void App::ensureStorageImage()
{
  std::cout << "ensureStorageImage() - Start" << std::endl;
  if (!storageImageInitialized || storageImageExtent != resolution)
    recreateStorageImage();
  std::cout << "ensureStorageImage() - End" << std::endl;
}

void App::recreateStorageImage()
{
  std::cout << "recreateStorageImage() - Start" << std::endl;
  std::cout << "Recreating storage image with resolution: " << resolution.x << "x" << resolution.y << std::endl;
  auto& ctx = etna::get_context();
  storageImageExtent = resolution;
  storageImage = ctx.createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "local_shadertoy1_storage_image",
    .format = storageImageFormat,
    .imageUsage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc |
      vk::ImageUsageFlagBits::eTransferDst,
  });
  storageImageInitialized = true;
  std::cout << "Storage image recreated" << std::endl;
  std::cout << "recreateStorageImage() - End" << std::endl;
}

std::pair<float, float> App::pullFrameTiming()
{
  std::cout << "pullFrameTiming() - Start" << std::endl;
  const double now = windowing.getTime();
  const float elapsed = static_cast<float>(now - startTimeSeconds);
  const float delta = static_cast<float>(now - lastFrameTimestamp);
  lastFrameTimestamp = now;
  
  std::cout << "pullFrameTiming() - now: " << now << ", elapsed: " << elapsed 
            << ", delta: " << delta << std::endl;
  std::cout << "pullFrameTiming() - End" << std::endl;
  
  return {elapsed, std::max(delta, 0.0f)};
}

glm::uvec2 App::calcDispatchGroupCount() const
{
  std::cout << "calcDispatchGroupCount() - Start" << std::endl;
  std::cout << "calcDispatchGroupCount() - resolution: " << resolution.x << "x" << resolution.y 
            << ", workgroupSize: " << workgroupSize.x << "x" << workgroupSize.y << std::endl;
  
  auto divUp = [](uint32_t total, uint32_t group_size) {
    std::cout << "calcDispatchGroupCount() - divUp: total=" << total << ", group_size=" << group_size << std::endl;
    if (group_size == 0) {
      std::cout << "ERROR: Division by zero in calcDispatchGroupCount!" << std::endl;
      return 0u;
    }
    uint32_t result = (total + group_size - 1u) / group_size;
    std::cout << "calcDispatchGroupCount() - divUp result: " << result << std::endl;
    return result;
  };

  if (workgroupSize.x == 0 || workgroupSize.y == 0) {
    std::cout << "ERROR: workgroupSize has zero component: " << workgroupSize.x << "x" << workgroupSize.y << std::endl;
    return {0, 0};
  }

  glm::uvec2 result = {
    divUp(resolution.x, workgroupSize.x),
    divUp(resolution.y, workgroupSize.y),
  };
  
  std::cout << "calcDispatchGroupCount() - Final result: " << result.x << "x" << result.y << std::endl;
  std::cout << "calcDispatchGroupCount() - End" << std::endl;
  
  return result;
}