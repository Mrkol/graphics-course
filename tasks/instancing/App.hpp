#pragma once
#include "wsi/OsWindowingManager.hpp"
#include "scene/Camera.hpp"
#include "Renderer.hpp"

class App
{
public:
  App();
  void run();

private:
  void processInput(float dt);
  void drawFrame();
  void moveCam(Camera& cam, const Keyboard& kb, float dt);
  void rotateCam(Camera& cam, const Mouse& mouse);

  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> mainWindow;
  Camera mainCam;
  std::unique_ptr<Renderer> renderer;
  float camMoveSpeed = 13.0f;
  float camRotateSpeed = 0.12f;
};
