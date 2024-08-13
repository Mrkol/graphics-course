#pragma once

#include <etna/GlobalContext.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <glm/glm.hpp>
#include <function2/function2.hpp>

#include "wsi/Keyboard.hpp"

#include "FramePacket.hpp"
#include "WorldRenderer.hpp"


class ImGuiRenderer;

using ResolutionProvider = fu2::unique_function<glm::uvec2() const>;

/**
 * This class encapsulates things that are very unlikely to change from one sample to another.
 * E.g. initialization, frame delivery logic, window resizing, gui setup, etc.
 */
class Renderer
{
public:
  explicit Renderer(glm::uvec2 resolution);
  ~Renderer();

  // Initializing all of rendering is a tricky multi-step dance
  void initVulkan(std::span<const char*> instance_extensions);
  void initFrameDelivery(vk::UniqueSurfaceKHR surface, ResolutionProvider res_provider);
  void recreateSwapchain(glm::uvec2 res);
  void loadScene(std::filesystem::path path);

  void debugInput(const Keyboard& kb);
  void update(const FramePacket& packet);
  void drawFrame();


private:
  ResolutionProvider resolutionProvider;
  std::unique_ptr<etna::Window> window;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  glm::uvec2 resolution;
  std::unique_ptr<ImGuiRenderer> guiRenderer;

  std::unique_ptr<WorldRenderer> worldRenderer;
};
