#include "App.hpp"
#include <tracy/Tracy.hpp>
#include <imgui.h>
#include "gui/ImGuiRenderer.hpp"

App::App()
{
  glm::uvec2 initialRes = {1280, 720};
  mainWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = initialRes,
  });
  renderer = std::make_unique<Renderer>(initialRes);
  renderer->initVulkan(windowing.getRequiredVulkanInstanceExtensions());
  auto surface = mainWindow->createVkSurface(etna::get_context().getInstance());
  renderer->initFrameDelivery(std::move(surface), [this]() { return mainWindow->getResolution(); });
  ImGuiRenderer::enableImGuiForWindow(mainWindow->native());
  mainCam.lookAt({55, 65, 85}, {0, 2, 0}, {0, 1, 0});
}

void App::run()
{
  double lastTime = windowing.getTime();
  while (!mainWindow->isBeingClosed())
  {
    const double now = windowing.getTime();
    const float dt = static_cast<float>(now - lastTime);
    lastTime = now;
    windowing.poll();
    processInput(dt);
    drawFrame();
    FrameMark;
  }
}

void App::processInput(float dt)
{
  if (mainWindow->keyboard[KeyboardKey::kEscape] == ButtonState::Falling)
    mainWindow->askToClose();

  if (mainWindow->mouse[MouseButton::mbRight] == ButtonState::Rising && !ImGui::GetIO().WantCaptureMouse)
    mainWindow->captureMouse = !mainWindow->captureMouse;

  camMoveSpeed = is_held_down(mainWindow->keyboard[KeyboardKey::kLeftShift]) ? 38.f : 13.f;
  if (!ImGui::GetIO().WantCaptureKeyboard || mainWindow->captureMouse)
    moveCam(mainCam, mainWindow->keyboard, dt);
  if (mainWindow->captureMouse)
    rotateCam(mainCam, mainWindow->mouse);
  renderer->debugInput(mainWindow->keyboard);
}

void App::drawFrame()
{
  renderer->update(FramePacket{
    .mainCam = mainCam,
    .currentTime = static_cast<float>(windowing.getTime()),
  });
  renderer->drawFrame();
}

void App::moveCam(Camera& cam, const Keyboard& kb, float dt)
{
  glm::vec3 dir{0.f, 0.f, 0.f};
  if (is_held_down(kb[KeyboardKey::kW])) dir += cam.forward();
  if (is_held_down(kb[KeyboardKey::kS])) dir -= cam.forward();
  if (is_held_down(kb[KeyboardKey::kD])) dir += cam.right();
  if (is_held_down(kb[KeyboardKey::kA])) dir -= cam.right();
  if (is_held_down(kb[KeyboardKey::kR])) dir += cam.up();
  if (is_held_down(kb[KeyboardKey::kF])) dir -= cam.up();
  if (glm::length(dir) > 1e-6f)
    cam.move(dt * camMoveSpeed * glm::normalize(dir));
}

void App::rotateCam(Camera& cam, const Mouse& mouse)
{
  cam.rotate(-camRotateSpeed * mouse.capturedPosDelta.y,
             -camRotateSpeed * mouse.capturedPosDelta.x);
  cam.fov = glm::clamp(cam.fov - 2.0f * mouse.scrollDelta.y, 1.0f, 120.0f);
}
