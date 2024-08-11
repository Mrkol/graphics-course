#include "OsWindowingManager.hpp"

#include <GLFW/glfw3.h>
#include <etna/Assert.hpp>


static OsWindowingManager* instance = nullptr;

void OsWindowingManager::onErrorCb(int /*errc*/, const char* message)
{
  spdlog::error("GLFW: {}", message);
}

void OsWindowingManager::onMouseScrollCb(GLFWwindow* window, double xoffset, double yoffset)
{
  if (auto it = instance->windows.find(window); it != instance->windows.end())
    it->second->mouse.scrollDelta = {xoffset, yoffset};
}

void OsWindowingManager::onWindowClosedCb(GLFWwindow* window)
{
  instance->windows.erase(window);
}

void OsWindowingManager::onWindowRefreshCb(GLFWwindow* window)
{
  if (auto it = instance->windows.find(window); it != instance->windows.end())
    if (it->second->onRefresh)
      it->second->onRefresh();
}

void OsWindowingManager::onWindowSizeCb(GLFWwindow* window, int width, int height)
{
  if (auto it = instance->windows.find(window); it != instance->windows.end())
    if (it->second->onResize)
      it->second->onResize({static_cast<glm::uint>(width),
        static_cast<glm::uint>(height)});
}

OsWindowingManager::OsWindowingManager()
{
  ETNA_ASSERT(glfwInit() == GLFW_TRUE);

  glfwSetErrorCallback(&OsWindowingManager::onErrorCb);

  ETNA_ASSERT(std::exchange(instance, this) == nullptr);
}

OsWindowingManager::~OsWindowingManager()
{
  ETNA_ASSERT(std::exchange(instance, nullptr) == this);

  glfwTerminate();
}

void OsWindowingManager::poll()
{
  glfwPollEvents();

  for (auto[_, window] : windows)
    updateWindow(*window);
}

double OsWindowingManager::getTime()
{
  return glfwGetTime();
}

std::unique_ptr<OsWindow> OsWindowingManager::createWindow(glm::uvec2 resolution,
  OsWindowRefreshCb refreshCb, OsWindowResizeCb resizeCb)
{
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  auto glfwWindow = glfwCreateWindow(static_cast<int>(resolution.x), static_cast<int>(resolution.y), "Sample", nullptr, nullptr);

  glfwSetScrollCallback(glfwWindow, &onMouseScrollCb);
  glfwSetWindowCloseCallback(glfwWindow, &onWindowClosedCb);
  glfwSetWindowRefreshCallback(glfwWindow, &onWindowRefreshCb);
  glfwSetWindowSizeCallback(glfwWindow, &onWindowSizeCb);

  auto result = std::unique_ptr<OsWindow>{new OsWindow};
  result->owner = this;
  result->impl = glfwWindow;
  result->onRefresh = std::move(refreshCb);
  result->onResize = std::move(resizeCb);

  windows.emplace(glfwWindow, result.get());
  return result;
}

std::span<const char*> OsWindowingManager::getRequiredVulkanInstanceExtensions()
{
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  return {glfwExtensions, glfwExtensionCount};
}

void OsWindowingManager::onWindowDestroyed(GLFWwindow* impl)
{
  instance->windows.erase(impl);
  glfwDestroyWindow(impl);
}

void OsWindowingManager::updateWindow(OsWindow& window)
{
  auto transitionState = [](ButtonState& state, bool glfwState) {
      switch (state)
      {
        case ButtonState::Low: if (glfwState) state = ButtonState::Rising; break;
        case ButtonState::Rising: state = ButtonState::High; break;
        case ButtonState::High: if (!glfwState) state = ButtonState::Falling; break;
        case ButtonState::Falling: state = ButtonState::Low; break;
        default: break;
      }
    };

  auto processMb = [&window, &transitionState](MouseButton mb, int glfwMb) {
    const bool pressed = glfwGetMouseButton(window.impl, glfwMb) == GLFW_PRESS;
    transitionState(window.mouse.buttons[static_cast<std::size_t>(mb)], pressed);
  };

  #define X(mb, glfwMb) processMb(MouseButton::mb, glfwMb);
  ALL_MOUSE_BUTTONS
  #undef X

  auto processKey = [&window, &transitionState](KeyboardKey key, int glfwKey) {
    const bool pressed = glfwGetKey(window.impl, glfwKey) == GLFW_PRESS;
    transitionState(window.keyboard.keys[static_cast<std::size_t>(key)], pressed);
  };

  #define X(mb, glfwMb) processKey(KeyboardKey::mb, glfwMb);
  ALL_KEYBOARD_KEYS
  #undef X

  if (window.captureMouse)
  {
    // Skip first frame on which mouse was captured so as not to
    // "jitter" the camera due to big offset from 0,0
    if (window.mouseWasCaptured)
    {
      double x;
      double y;
      glfwGetCursorPos(window.impl, &x, &y);
      glfwSetCursorPos(window.impl, 0, 0);

      window.mouse.posDelta = {x, y};
    }
    else
    {
      glfwSetInputMode(window.impl, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      glfwSetCursorPos(window.impl, 0, 0);
      window.mouseWasCaptured = true;
    }
  }
  else
  {
    glfwSetInputMode(window.impl, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    window.mouseWasCaptured = false;
  }
}
