#include "Renderer.hpp"
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/Profiling.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cstdlib>

Renderer::Renderer(glm::uvec2 res) : resolution{res} {}

void Renderer::initVulkan(std::span<const char*> instance_extensions)
{
  std::vector<const char*> extensions(instance_extensions.begin(), instance_extensions.end());
  const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  etna::initialize(etna::InitParams{
    .applicationName = "instancing",
    .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
    .instanceExtensions = extensions,
    .deviceExtensions = deviceExtensions,
    .features = vk::PhysicalDeviceFeatures2{.features = {}},
    .numFramesInFlight = 2,
  });
}

void Renderer::initFrameDelivery(vk::UniqueSurfaceKHR surface, ResolutionProvider provider)
{
  resolutionProvider = std::move(provider);
  auto& ctx = etna::get_context();
  commandManager = ctx.createPerFrameCmdMgr();
  window = ctx.createWindow(etna::Window::CreateInfo{.surface = std::move(surface)});
  auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
    .resolution = {resolution.x, resolution.y},
    .vsync = true,
    .numFramesInFlight = static_cast<uint32_t>(commandManager->getCmdBufferCount()),
  });
  resolution = {w, h};
  worldRenderer = std::make_unique<WorldRenderer>();
  worldRenderer->allocateResources(resolution);
  worldRenderer->loadShaders();
  worldRenderer->setupPipelines(window->getCurrentFormat());
  guiRenderer = std::make_unique<ImGuiRenderer>(window->getCurrentFormat());
}

void Renderer::recreateSwapchain(glm::uvec2 res)
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
  auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
    .resolution = {res.x, res.y},
    .vsync = true,
    .numFramesInFlight = static_cast<uint32_t>(commandManager->getCmdBufferCount()),
  });
  resolution = {w, h};
  worldRenderer->resizeDepth(resolution);
  worldRenderer->setupPipelines(window->getCurrentFormat());
}

void Renderer::debugInput(const Keyboard& kb)
{
  if (kb[KeyboardKey::kB] != ButtonState::Falling)
    return;
  const int result = std::system("cd \"" GRAPHICS_COURSE_ROOT "/build\" && cmake --build . --target instancing_shaders");
  if (result == 0)
  {
    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
    etna::reload_shaders();
    spdlog::info("Instancing shaders reloaded");
  }
  else
    spdlog::warn("Shader recompilation failed");
}

void Renderer::update(const FramePacket& packet) { worldRenderer->update(packet); }

void Renderer::drawFrame()
{
  guiRenderer->nextFrame();
  ImGui::NewFrame();
  worldRenderer->drawGui();
  ImGui::Render();

  auto cmd = commandManager->acquireNext();
  etna::begin_frame();
  auto nextImage = window->acquireNext();
  if (nextImage)
  {
    auto [image, view, availableSem, readySem] = *nextImage;
    ETNA_CHECK_VK_RESULT(cmd.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(cmd, instancingFrame);
      worldRenderer->renderWorld(cmd, image, view, uint32_t(submittedFrames % 2));
      guiRenderer->render(cmd, {{0, 0}, {resolution.x, resolution.y}},
                          image, view, ImGui::GetDrawData());
      etna::set_state(cmd, image, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                      {}, vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(cmd);
      ETNA_READ_BACK_GPU_PROFILING(cmd);
    }
    ETNA_CHECK_VK_RESULT(cmd.end());
    auto done = commandManager->submit(std::move(cmd), std::move(availableSem), std::move(readySem));
    ++submittedFrames;
    if (!window->present(std::move(done), view))
      nextImage = std::nullopt;
  }
  etna::end_frame();
  if (!nextImage)
  {
    const auto res = resolutionProvider();
    if (res.x && res.y) recreateSwapchain(res);
  }
}

Renderer::~Renderer()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}
