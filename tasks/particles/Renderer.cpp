#include "Renderer.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/Profiling.hpp>
#include <imgui.h>


Renderer::Renderer(glm::uvec2 res)
  : resolution{res}
{
}

void Renderer::initVulkan(std::span<const char*> instance_extensions)
{
  std::vector<const char*> instanceExtensions;

  for (auto ext : instance_extensions)
    instanceExtensions.push_back(ext);
  
  instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");

  std::vector<const char*> deviceExtensions;

  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  deviceExtensions.push_back("VK_KHR_maintenance3");
  deviceExtensions.push_back("VK_KHR_shader_draw_parameters");
  // deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
  // deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME);

  // vk::PhysicalDeviceDynamicRenderingLocalReadFeatures dynamicLocalReadFeatures {
  //   .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
  //   .pNext = NULL,
  //   .dynamicRenderingLocalRead = VK_TRUE
  // };

  vk::PhysicalDeviceVulkan12Features features12 {
    .sType = vk::StructureType::ePhysicalDeviceVulkan12Features,
    .pNext = NULL,
    .shaderSampledImageArrayNonUniformIndexing = 1,
    .descriptorBindingPartiallyBound = 1,
    .descriptorBindingVariableDescriptorCount = 1,
    .runtimeDescriptorArray = 1
  };


  etna::initialize(etna::InitParams{
    .applicationName = "particles_renderer",
    .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
    .instanceExtensions = instanceExtensions,
    .deviceExtensions = deviceExtensions,
    .features = vk::PhysicalDeviceFeatures2{
      .sType = vk::StructureType::ePhysicalDeviceFeatures2,
      .pNext = &features12,
      .features = {
        .multiDrawIndirect = 1,
        .shaderSampledImageArrayDynamicIndexing = 1
      }
    },
    .physicalDeviceIndexOverride = {},
    .numFramesInFlight = 2,
  });
}

void Renderer::initFrameDelivery(vk::UniqueSurfaceKHR a_surface, ResolutionProvider res_provider)
{
  resolutionProvider = std::move(res_provider);

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

  guiRenderer = std::make_unique<ImGuiRenderer>(window->getCurrentFormat());
}

void Renderer::loadScene(std::filesystem::path path)
{
  scenePath = path;
  worldRenderer->loadScene(path);
}

void Renderer::debugInput(const Keyboard& kb)
{
  worldRenderer->debugInput(kb);

  if (kb[KeyboardKey::kB] == ButtonState::Falling)
  {
    const int retval = std::system("cd " GRAPHICS_COURSE_ROOT "/build"
                                   " && cmake --build . --target model_bakery_renderer_shaders");
    if (retval != 0)
      spdlog::warn("Shader recompilation returned a non-zero return code!");
    else
    {
      ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
      etna::reload_shaders();
      spdlog::info("Successfully reloaded shaders!");
    }
  }
}

void Renderer::drawGui() {
  ImGui::Begin("Simple render settings");

  auto inputPathText = [](std::filesystem::path& path, const char* name) {
    char path_input[4096];
    // Couldn't get it all to work with cyrillic characters, so ASCII it is.
    std::string pathAsString = path.string();
    strcpy_s(path_input, sizeof(path_input), (const char*)pathAsString.c_str());
    ImGui::InputText(name, path_input, sizeof(path_input));
    path = std::filesystem::path(path_input);

    ImGui::SetItemTooltip("Please, only use ASCII paths.");
  };

  inputPathText(scenePath, "Path to scene to load");

  if (ImGui::Button("Reload scene")) {
    needToReload = true;  // Can't reload right now, Vulkan would whine.
  }

  ImGui::Text(
    "Application average %.3f ms/frame (%.1f FPS)",
    1000.0f / ImGui::GetIO().Framerate,
    ImGui::GetIO().Framerate);

  ImGui::NewLine();

  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press 'B' to recompile and reload shaders");
  ImGui::End();
}

void Renderer::update(const FramePacket& packet)
{
  worldRenderer->update(packet);
}

void Renderer::drawFrame()
{
  ZoneScoped;

  auto currentCmdBuf = commandManager->acquireNext();

  {
    guiRenderer->nextFrame();
    ImGui::NewFrame();
    drawGui();
    ImGui::Render();
  }

  etna::begin_frame();

  auto nextSwapchainImage = window->acquireNext();

  if (nextSwapchainImage)
  {
    auto [image, view, availableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      ETNA_PROFILE_GPU(currentCmdBuf, renderFrame);

      if (needToReload) {
        vk::Result err = etna::get_context().getQueue().waitIdle();
        if (err != vk::Result::eSuccess) {
          spdlog::error("Can't wait for queue! Some error happened!");
        } else {
          worldRenderer->unbindScene(currentCmdBuf);
          loadScene(scenePath);
        }
        needToReload = false;
      }

      worldRenderer->renderWorld(currentCmdBuf, image, view);

      {
        ImDrawData* pDrawData = ImGui::GetDrawData();
        guiRenderer->render(
          currentCmdBuf, {{0, 0}, {resolution.x, resolution.y}}, image, view, pDrawData);
      }

      etna::set_state(
        currentCmdBuf,
        image,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
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

  if (!nextSwapchainImage && resolutionProvider() != glm::uvec2{0, 0})
  {
    auto [w, h] = window->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }

  etna::end_frame();
}

Renderer::~Renderer()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}
