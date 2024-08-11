#pragma once

#include "wsi/OsWindowingManager.hpp"
#include "scene/Camera.h"

#include "Renderer.h"


class App
{
public:
  App();

  void run();

private:
  void processInput(float dt);

  void moveCam(Camera& cam, const Keyboard& kb, float dt);
  void rotateCam(Camera& cam, const Mouse& ms, float dt);

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> mainWindow;

  float camMoveSpeed = 1;
  float camRotateSpeed = 0.1f;
  float zoomSensitivity = 0.2f;
  Camera mainCam;
  Camera shadowCam;

  bool controlShadowCam = false;

  std::unique_ptr<Renderer> renderer;
};
