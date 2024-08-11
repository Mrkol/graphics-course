#pragma once

#include <memory>
#include <span>
#include <unordered_map>

#include "wsi/OsWindow.hpp"


class OsWindowingManager
{
  friend class OsWindow;

public:
  OsWindowingManager();
  ~OsWindowingManager();

  OsWindowingManager(const OsWindowingManager&) = delete;
  OsWindowingManager& operator=(const OsWindowingManager&) = delete;

  OsWindowingManager(OsWindowingManager&&) = delete;
  OsWindowingManager& operator=(OsWindowingManager&&) = delete;


  void poll();

  double getTime();

  /**
   * Creates a new OS-native window.
   * NOTE: you MUST use refreshCb to re-draw your frame to get
   * cross-platform window resizing behavior.
   * NOTE: you MUST use resizeCb to recreate your swapchain to get
   * cross-platform window resizing behavior.
   */
  std::unique_ptr<OsWindow> createWindow(
    glm::uvec2 resolution, OsWindowRefreshCb refresh_cb, OsWindowResizeCb resize_cb);

  std::span<const char*> getRequiredVulkanInstanceExtensions();

private:
  void updateWindow(OsWindow& window);

  static void onErrorCb(int errc, const char* message);
  static void onMouseScrollCb(GLFWwindow* window, double xoffset, double yoffset);
  static void onWindowClosedCb(GLFWwindow* window);
  static void onWindowRefreshCb(GLFWwindow* window);
  static void onWindowSizeCb(GLFWwindow* window, int width, int height);

  static void onWindowDestroyed(GLFWwindow*);

private:
  std::unordered_map<GLFWwindow*, OsWindow*> windows;
};
