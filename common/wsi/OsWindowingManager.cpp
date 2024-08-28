#include "OsWindowingManager.hpp"

#include <GLFW/glfw3.h>
#include <etna/Assert.hpp>

#include <tracy/Tracy.hpp>


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
      it->second->onResize({static_cast<glm::uint>(width), static_cast<glm::uint>(height)});
}

OsWindowingManager::OsWindowingManager()
{
  ETNA_VERIFY(glfwInit() == GLFW_TRUE);

  glfwSetErrorCallback(&OsWindowingManager::onErrorCb);

  ETNA_VERIFY(std::exchange(instance, this) == nullptr);
}

OsWindowingManager::~OsWindowingManager()
{
  ETNA_VERIFY(std::exchange(instance, nullptr) == this);

  glfwTerminate();
}

void OsWindowingManager::poll()
{
  ZoneScoped;

  for (auto [_, window] : windows)
    window->mouse.scrollDelta = {0, 0};

  glfwPollEvents();

  for (auto [_, window] : windows)
    updateWindow(*window);
}

double OsWindowingManager::getTime()
{
  return glfwGetTime();
}

std::unique_ptr<OsWindow> OsWindowingManager::createWindow(OsWindow::CreateInfo info)
{
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, info.resizeable ? GLFW_TRUE : GLFW_FALSE);

  auto glfwWindow = glfwCreateWindow(
    static_cast<int>(info.resolution.x),
    static_cast<int>(info.resolution.y),
    "Sample",
    nullptr,
    nullptr);

  glfwSetScrollCallback(glfwWindow, &onMouseScrollCb);
  glfwSetWindowCloseCallback(glfwWindow, &onWindowClosedCb);
  glfwSetWindowRefreshCallback(glfwWindow, &onWindowRefreshCb);
  glfwSetWindowSizeCallback(glfwWindow, &onWindowSizeCb);

  auto result = std::unique_ptr<OsWindow>{new OsWindow};
  result->owner = this;
  result->impl = glfwWindow;
  result->onRefresh = std::move(info.refreshCb);
  result->onResize = std::move(info.resizeCb);

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
  auto transitionState = [](ButtonState& state, bool glfw_state) {
    switch (state)
    {
    case ButtonState::Low:
      if (glfw_state)
        state = ButtonState::Rising;
      break;
    case ButtonState::Rising:
      state = ButtonState::High;
      break;
    case ButtonState::High:
      if (!glfw_state)
        state = ButtonState::Falling;
      break;
    case ButtonState::Falling:
      state = ButtonState::Low;
      break;
    default:
      break;
    }
  };

  auto processMb = [&window, &transitionState](MouseButton mb, int glfw_mb) {
    const bool pressed = glfwGetMouseButton(window.impl, glfw_mb) == GLFW_PRESS;
    transitionState(window.mouse.buttons[static_cast<std::size_t>(mb)], pressed);
  };

#define X(mb, glfwMb) processMb(MouseButton::mb, glfwMb);
  ALL_MOUSE_BUTTONS
#undef X

  auto processKey = [&window, &transitionState](KeyboardKey key, int glfw_key) {
    const bool pressed = glfwGetKey(window.impl, glfw_key) == GLFW_PRESS;
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

      window.mouse.capturedPosDelta = {x, y};
    }
    else
    {
      glfwSetInputMode(window.impl, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      glfwSetCursorPos(window.impl, 0, 0);
      window.mouseWasCaptured = true;
    }

    window.mouse.freePos = {0, 0};
  }
  else
  {
    glfwSetInputMode(window.impl, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    window.mouseWasCaptured = false;

    double x;
    double y;
    glfwGetCursorPos(window.impl, &x, &y);
    window.mouse.freePos = {x, y};
    window.mouse.capturedPosDelta = {0, 0};
  }
}
