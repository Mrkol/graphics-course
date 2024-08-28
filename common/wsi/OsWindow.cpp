#include "OsWindow.hpp"

#include <GLFW/glfw3.h>
#include <etna/Assert.hpp>

#include "OsWindowingManager.hpp"


OsWindow::~OsWindow()
{
  OsWindowingManager::onWindowDestroyed(impl);
}

void OsWindow::askToClose()
{
  glfwSetWindowShouldClose(impl, GLFW_TRUE);
}

bool OsWindow::isBeingClosed()
{
  return glfwWindowShouldClose(impl) == GLFW_TRUE;
}

glm::uvec2 OsWindow::getResolution()
{
  glm::ivec2 result;
  glfwGetWindowSize(impl, &result.x, &result.y);
  ETNA_VERIFY(result.x >= 0 && result.y >= 0);
  return glm::uvec2(result);
}

vk::UniqueSurfaceKHR OsWindow::createVkSurface(vk::Instance instance)
{
  VkSurfaceKHR surface;
  auto cres = glfwCreateWindowSurface(instance, impl, nullptr, &surface);
  ETNA_CHECK_VK_RESULT(static_cast<vk::Result>(cres));

  return vk::UniqueSurfaceKHR{
    vk::SurfaceKHR{surface},
    vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>{instance}};
}
