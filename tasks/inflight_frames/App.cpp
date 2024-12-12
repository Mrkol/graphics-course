#include "App.hpp"

#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>

#include "stb_image.h"

#include <tracy/Tracy.hpp>

#include <etna/Profiling.hpp>
#include <iostream>


App::App()
  : resolution{1280, 720}
  , useVsync{true}
  , timeStart{std::chrono::system_clock::now()}
{
  {
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    etna::initialize(etna::InitParams{
      .applicationName = "Inflight Frames",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 2,
    });
  }

  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });
  vk::Format winFormat;

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
    winFormat = vkWindow->getCurrentFormat();
  }

  commandManager = etna::get_context().createPerFrameCmdMgr();
  
  etna::create_program(
    "texture", 
    {INFLIGHT_FRAMES_SHADERS_ROOT "shader.vert.spv",
     INFLIGHT_FRAMES_SHADERS_ROOT "texture.frag.spv"});

  etna::create_program(
    "shader",
    {INFLIGHT_FRAMES_SHADERS_ROOT "shader.vert.spv",
     INFLIGHT_FRAMES_SHADERS_ROOT "toy.frag.spv"});

  renderer.initPipelines(resolution, winFormat);


  auto oneShotManager = etna::get_context().createOneShotCmdMgr();
  renderer.loadResource(*oneShotManager, GRAPHICS_COURSE_RESOURCES_ROOT "/textures/test_tex_1.png");

  cam.position = {0.0, 0.0, 6.0};
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

  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::drawFrame()
{
  ZoneScoped;

  float frameTime = std::chrono::duration<float, std::ratio<1,1>>(std::chrono::high_resolution_clock::now() - timeStart).count();
  
  auto currentCmdBuf = commandManager->acquireNext();
  etna::begin_frame();
  auto nextSwapchainImage = vkWindow->acquireNext();
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, renderFrame);

      renderer.render(currentCmdBuf, backbuffer, backbufferView, static_cast<uint32_t>(sizeof(frameTime)), &frameTime);

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

void App::rotateCam(const Mouse& ms) {
  cam.move({ms.capturedPosDelta.x, ms.capturedPosDelta.y, ms.scrollDelta.y});
}

void 
App::processInput() 
{
  if (osWindow->mouse[MouseButton::mbRight] == ButtonState::Rising)
    osWindow->captureMouse = !osWindow->captureMouse;

  if (osWindow->captureMouse) {
    rotateCam(osWindow->mouse);
  }

  std::cerr << cam.position.x << " " << cam.position.y << " " << cam.position.z << std::endl;
  cam.position.y += 0.01f;
  renderer.update(cam.position);
}