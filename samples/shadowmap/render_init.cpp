#include "Renderer.h"

#include <etna/Etna.hpp>

#include "gui/ImGuiRenderer.hpp"


Renderer::Renderer(glm::uvec2 res) : resolution{res}
{
  uniformParams.baseColor = {0.9f, 0.92f, 1.0f};
}

void Renderer::initVulkan(std::span<const char*> instance_extensions)
{
  std::vector<const char*> m_instanceExtensions;

  for (auto ext : instance_extensions)
    m_instanceExtensions.push_back(ext);

  #ifndef NDEBUG
    m_instanceExtensions.push_back("VK_EXT_debug_report");
  #endif

  std::vector<const char*> m_deviceExtensions;

  m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  etna::initialize(etna::InitParams
    {
      .applicationName = "ShadowmapSample",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = m_instanceExtensions,
      .deviceExtensions = m_deviceExtensions,
      .features = vk::PhysicalDeviceFeatures2
        {
          .features = {}
        },
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      // How much frames we buffer on the GPU without waiting for their completion on the CPU
      .numFramesInFlight = 2
    });

  context = &etna::get_context();

  sceneMgr = std::make_unique<SceneManager>();
}

void Renderer::initPresentation(vk::UniqueSurfaceKHR a_surface, ResolutionProvider res_provider)
{
  resolutionProvider = std::move(res_provider);
  commandManager = context->createPerFrameCmdMgr();

  window = context->createWindow(etna::Window::CreateInfo{
    .surface = std::move(a_surface),
  });

  auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
    .resolution = {resolution.x, resolution.y},
    .vsync = true,
  });
  resolution = {w, h};

  guiRenderer = std::make_unique<ImGuiRenderer>(window->getCurrentFormat());

  allocateResources();
}

void Renderer::recreateSwapchain(glm::uvec2 res)
{
  ETNA_CHECK_VK_RESULT(context->getDevice().waitIdle());

  auto[w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {res.x, res.y},
      .vsync = true,
    });
  resolution = {w, h};

  // Most resources depend on the current resolution, so we recreate them.
  allocateResources();

  // NOTE: if swapchain changes format (that can happen on android),
  // we will probably die, as we render to swapchain in a bunch of places
  // but don't recreate pipelines which use it's format.
  // Can be fixed, but we don't care about android for now.
}

Renderer::~Renderer()
{
  ETNA_CHECK_VK_RESULT(context->getDevice().waitIdle());
}
