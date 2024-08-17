#pragma once

#include <etna/Vulkan.hpp>
#include <function2/function2.hpp>

#include "wsi/Keyboard.hpp"
#include "wsi/Mouse.hpp"


struct GLFWwindow;
class OsWindowingManager;

// On windows, polling blocks when a window is being resized,
// so a refresh/redraw/repaint callback must be used to retain
// interactivity.
using OsWindowRefreshCb = fu2::unique_function<void()>;

// OS-driven resize callbacks must be used on wayland to get
// proper resizing of windows, vulkan-only tools do not suffice.
using OsWindowResizeCb = fu2::unique_function<void(glm::uvec2)>;

class OsWindow
{
  friend class OsWindowingManager;

  OsWindow() {}

public:
  struct CreateInfo
  {
    glm::uvec2 resolution;

    // If you mark this as true, you MUST use both refreshCb and resizeCb
    // for resizing to properly work on all platforms.
    bool resizeable = false;

    // Use this to redraw your application frame while the
    // window is being resized.
    OsWindowRefreshCb refreshCb = {};

    // Use this to save the new resolution and recreate various
    // resolution-dependent resources while the window is being resized.
    OsWindowResizeCb resizeCb = {};
  };

  OsWindow(const OsWindow&) = delete;
  OsWindow& operator=(const OsWindow&) = delete;

  OsWindow(OsWindow&&) = delete;
  OsWindow& operator=(OsWindow&&) = delete;

  ~OsWindow();


  // Manually ask to quit
  void askToClose();

  // Might be triggered by the user pressing the "cross" button
  bool isBeingClosed();

  // Returns current resolution. Might be 0,0 if minimized
  glm::uvec2 getResolution();

  vk::UniqueSurfaceKHR createVkSurface(vk::Instance instance);

  GLFWwindow* native() const { return impl; }

  bool captureMouse = false;
  Keyboard keyboard;
  Mouse mouse;

private:
  OsWindowingManager* owner = nullptr;
  GLFWwindow* impl = nullptr;
  OsWindowResizeCb onResize;
  OsWindowRefreshCb onRefresh;
  bool mouseWasCaptured = false;
};
