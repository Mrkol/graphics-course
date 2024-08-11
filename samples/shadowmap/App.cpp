#include "App.hpp"

#include "gui/ImGuiRenderer.hpp"


App::App()
{
  glm::uvec2 initialRes = {1280, 720};
  mainWindow = windowing.createWindow(initialRes,
    [this](){
      // NOTE: this is only called when the window is being resized.
      renderer->updateView(mainCam, shadowCam);
      renderer->drawFrame(static_cast<float>(windowing.getTime()));
    },
    [this](glm::uvec2 res){
      if (res.x == 0 || res.y == 0)
        return;

      renderer->recreateSwapchain(res);
    });

  renderer.reset(new Renderer(initialRes));

  auto instExts = windowing.getRequiredVulkanInstanceExtensions();
  renderer->initVulkan(instExts);

  auto surface = mainWindow->createVkSurface(etna::get_context().getInstance());

  renderer->initPresentation(std::move(surface), [window = mainWindow.get()]() {
      return window->getResolution();
  });

  // TODO: this is bad design, this initialization is dependent on the current ImGui context, but we pass
  // it implicitly here instead of explicitly. Beware if trying to do something tricky.
  ImGuiRenderer::enableImGuiForWindow(mainWindow->native());

  shadowCam.lookAt({-8, 10, 8}, {0, 0, 0}, {0, 1, 0});
  mainCam.lookAt({0, 10, 10}, {0, 0, 0}, {0, 1, 0});

  renderer->loadScene(GRAPHICS_COURSE_ROOT "/resources/scenes/low_poly_dark_town/scene.gltf");
}

void App::run()
{
  double lastTime = windowing.getTime();
  while (!mainWindow->isBeingClosed())
  {
    const double currTime = windowing.getTime();
    const float diffTime = static_cast<float>(currTime - lastTime);
    lastTime = currTime;

    windowing.poll();

    processInput(diffTime);

    renderer->debugInput(mainWindow->keyboard);

    renderer->updateView(mainCam, shadowCam);

    renderer->drawFrame(static_cast<float>(currTime));
  }
}

void App::processInput(float dt)
{
  if (mainWindow->keyboard[KeyboardKey::kEscape] == ButtonState::Falling)
    mainWindow->askToClose();

  if (is_held_down(mainWindow->keyboard[KeyboardKey::kLeftShift]))
    camMoveSpeed = 10;
  else
    camMoveSpeed = 1;

  if (mainWindow->keyboard[KeyboardKey::kL] == ButtonState::Falling)
    controlShadowCam = !controlShadowCam;

  if (mainWindow->mouse[MouseButton::mbRight] == ButtonState::Rising)
    mainWindow->captureMouse = !mainWindow->captureMouse;

  auto &camToControl = controlShadowCam ? shadowCam : mainCam;

  moveCam(camToControl, mainWindow->keyboard, dt);
  if (mainWindow->captureMouse)
    rotateCam(camToControl, mainWindow->mouse, dt);
}

void App::moveCam(Camera& cam, const Keyboard& kb, float dt)
{
  // Move position of camera based on WASD keys, and FR keys for up and down

  glm::vec3 dir = {0, 0, 0};

  if (is_held_down(kb[KeyboardKey::kS]))
    dir -= cam.forward();

  if (is_held_down(kb[KeyboardKey::kW]))
    dir += cam.forward();

  if (is_held_down(kb[KeyboardKey::kA]))
    dir -= cam.right();

  if (is_held_down(kb[KeyboardKey::kD]))
    dir += cam.right();

  if (is_held_down(kb[KeyboardKey::kF]))
    dir -= cam.up();

  if (is_held_down(kb[KeyboardKey::kR]))
    dir += cam.up();

  // NOTE: This is how you make moving diagonally not be faster than
  // in a straight line.
  cam.move(dt * camMoveSpeed * (length(dir) > 1e-9 ? normalize(dir) : dir));
}

void App::rotateCam(Camera& cam, const Mouse& ms, float /*dt*/)
{
  // TODO: should dt be accounted for here?

  // Rotate camera based on mouse movement
  cam.rotate(camRotateSpeed * ms.posDelta.y, camRotateSpeed * ms.posDelta.x);

  // Increase or decrease field of view based on mouse wheel
  cam.fov -= zoomSensitivity * ms.scrollDelta.y;
  if (cam.fov < 1.0f)
    cam.fov = 1.0f;
  if (cam.fov > 180.0f)
    cam.fov = 180.0f;
}
