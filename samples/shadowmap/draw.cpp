#include "Renderer.h"

#include <etna/Etna.hpp>
#include <imgui.h>

#include "gui/ImGuiRenderer.hpp"


void Renderer::drawFrame(bool draw_gui)
{
  auto currentCmdBuf = commandManager->acquireNext();

  // TODO: this makes literally 0 sense here, rename/refactor,
  // it doesn't actually begin anything, just resets descriptor pools
  etna::begin_frame();

  auto nextSwapchainImage = window->acquireNext();

  // NOTE: here, we skip frames when the window is in the process of being
  // re-sized. This is not mandatory, it is possible to submit frames to a
  // "sub-optimal" swap chain and still get something drawn while resizing,
  // but only on some platforms (not windows+nvidia, sadly).
  if (nextSwapchainImage)
  {
    auto[image, view, availableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin({ .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse }));
    renderWorld(currentCmdBuf, image, view);

    if (draw_gui)
    {
      ImDrawData* pDrawData = ImGui::GetDrawData();
      guiRenderer->render(currentCmdBuf, {{0, 0}, {resolution.x, resolution.y}}, image, view, pDrawData);
    }

    etna::set_state(currentCmdBuf, image, vk::PipelineStageFlagBits2::eBottomOfPipe, {},
      vk::ImageLayout::ePresentSrcKHR, vk::ImageAspectFlagBits::eColor);

    etna::flush_barriers(currentCmdBuf);

    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    auto renderingDone = commandManager->submit(std::move(currentCmdBuf), std::move(availableSem));

    const bool presented = window->present(std::move(renderingDone), view);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  if (!nextSwapchainImage)
  {
    auto res = resolutionProvider();
    // On windows, we get 0,0 while the window is minimized and
    // must skip frames until the window is un-minimized again
    if (res.x != 0 && res.y != 0)
      recreateSwapchain(res);
  }
}

void Renderer::drawFrame(float time)
{
  updateUniformBuffer(time);
  DrawGui();
  drawFrame(true);
}
