#include "App.hpp"

#include <chrono>
#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <ratio>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

#include <tracy/Tracy.hpp>

#include <etna/Profiling.hpp>


App::App()
// Divide resolution to increase FPS <:)
  : resolution{1280 / 2, 720 / 2}
  , useVsync{true}
{

  timeStart = std::chrono::high_resolution_clock::now();
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for jus../tasks/inflight_frames/App.cppt about anything.
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
    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 2,
    });
  }

  // Now we can create an OS window
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });
  vk::Format winFormat;
  // But we also need to hook the OS window up to Vulkan manually!
  {
    // First, we ask GLFW to provide a "surface" for the window,
    // which is an opaque description of the area where we can actually render.
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    // Then we pass it to Etna to do the complicated work for us
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    // And finally ask Etna to create the actual swapchain so that we can
    // get (different) images each frame to render stuff into.
    // Here, we do not support window resizing, so we only need to call this once.
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    // Technically, Vulkan might fail to initialize a swapchain with the requested
    // resolution and pick a different one. This, however, does not occur on platforms
    // we support. Still, it's better to follow the "intended" path.
    resolution = {w, h};
    winFormat = vkWindow->getCurrentFormat();
  }

  // Next, we need a magical Etna helper to send commands to the GPU.
  // How it is actually performed is not trivial, but we can skip this for now.
  commandManager = etna::get_context().createPerFrameCmdMgr();

  etna::create_program("toy_shader", 
    {
      INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv",
      INFLIGHT_FRAMES_SHADERS_ROOT "toy.frag.spv"
    });

  etna::create_program("skybox_shader", 
    {
      INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv",
      INFLIGHT_FRAMES_SHADERS_ROOT "skybox.frag.spv"
    });
  
  renderer.initPipelines(resolution, winFormat);

  auto oneShotManager = etna::get_context().createOneShotCmdMgr();
  renderer.loadResource(*oneShotManager, RESOURCE_DIR "textures/shadertoyTex0.jpg");
  renderer.loadResource(*oneShotManager, RESOURCE_DIR "textures/shadertoyTex1.jpg");
  renderer.loadResource(*oneShotManager, RESOURCE_DIR "textures/shadertoyTex2.jpg");
  renderer.loadResource(*oneShotManager, RESOURCE_DIR "textures/shadertoyTex3.jpg");
  renderer.loadResource(*oneShotManager, RESOURCE_DIR "textures/shadertoyTex4.png");

  cam.lookAt({0., 0., -10.}, {0., 0., 0.}, {0., -1., 0.});
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
    
    processInput();
   
    drawFrame();

    FrameMark;
  }

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame()
{
  ZoneScoped;
  // This is cursed but I don't care;
  float frameTime = std::chrono::duration<float, std::ratio<1,1>>(std::chrono::high_resolution_clock::now() - timeStart).count();

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

    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, renderFrame);

      renderer.render(currentCmdBuf, backbuffer, backbufferView, static_cast<uint32_t>(sizeof(frameTime)), &frameTime);

      // At the end of "rendering", we are required to change how the pixels of the
      // swpchain image are laid out in memory to something that is appropriate
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
      ETNA_READ_BACK_GPU_PROFILING(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    // Note that the GPU won't start executing our commands before the semaphore is
    // signalled, which will happen when the OS says that the next swapchain image is ready.
    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

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
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}

void App::rotateCam(const Mouse& ms) {
  cam.rotate(ms.capturedPosDelta.y, ms.capturedPosDelta.x);
  // Increase or decrease field of view based on mouse wheel
  cam.fov -= ms.scrollDelta.y;
  if (cam.fov < 1.0f)
    cam.fov = 1.0f;
  if (cam.fov > 120.0f)
    cam.fov = 120.0f;
}

void 
App::processInput() 
{
  if (osWindow->mouse[MouseButton::mbRight] == ButtonState::Rising)
    osWindow->captureMouse = !osWindow->captureMouse;

  if (osWindow->captureMouse) {
    rotateCam(osWindow->mouse);
  }

  renderer.update(cam.forward());
}