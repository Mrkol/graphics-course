#include "Renderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <imgui.h>

#include <gui/ImGuiRenderer.hpp>


Renderer::Renderer(glm::uvec2 res)
  : resolution{res}
{
}

void Renderer::initVulkan(std::span<const char*> instance_extensions)
{
  std::vector<const char*> instanceExtensions;

  for (auto ext : instance_extensions)
    instanceExtensions.push_back(ext);

#ifndef NDEBUG
  instanceExtensions.push_back("VK_EXT_debug_report");
#endif

  std::vector<const char*> deviceExtensions;

  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  etna::initialize(etna::InitParams{
    .applicationName = "model_bakery_renderer",
    .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
    .instanceExtensions = instanceExtensions,
    .deviceExtensions = deviceExtensions,
    .features = vk::PhysicalDeviceFeatures2{.features = {}},
    .physicalDeviceIndexOverride = {},
    .numFramesInFlight = 2,
  });
}

void Renderer::initFrameDelivery(vk::UniqueSurfaceKHR a_surface)
{
  auto& ctx = etna::get_context();

  commandManager = ctx.createPerFrameCmdMgr();

  window = ctx.createWindow(etna::Window::CreateInfo{
    .surface = std::move(a_surface),
  });

  auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
    .resolution = {resolution.x, resolution.y},
    .vsync = useVsync,
  });

  resolution = {w, h};

  worldRenderer = std::make_unique<WorldRenderer>();

  worldRenderer->allocateResources(resolution);
  worldRenderer->loadShaders();
  worldRenderer->setupPipelines(window->getCurrentFormat());
}

void Renderer::loadScene(std::filesystem::path path)
{
  worldRenderer->loadScene(path);
}

void Renderer::debugInput(const Keyboard& kb)
{
  worldRenderer->debugInput(kb);

  if (kb[KeyboardKey::kB] == ButtonState::Falling)
  {
    std::system("cd " GRAPHICS_COURSE_ROOT "/build"
                " && cmake --build . --target shadowmap_shaders");
    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
    etna::reload_shaders();
  }
}

void Renderer::update(const FramePacket& packet)
{
  worldRenderer->update(packet);
}

void Renderer::drawFrame()
{
  ZoneScoped;

  auto currentCmdBuf = commandManager->acquireNext();

  etna::begin_frame();

  auto nextSwapchainImage = window->acquireNext();

  if (nextSwapchainImage)
  {
    auto [image, view, availableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, renderFrame);

      worldRenderer->renderWorld(currentCmdBuf, image, view);

      etna::set_state(
        currentCmdBuf,
        image,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);

      etna::flush_barriers(currentCmdBuf);

      ETNA_READ_BACK_GPU_PROFILING(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(availableSem));

    const bool presented = window->present(std::move(renderingDone), view);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();
}

Renderer::~Renderer()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}
