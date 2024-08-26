#pragma once

#include <etna/Vulkan.hpp>


struct ImDrawData;
struct ImGuiContext;
struct GLFWwindow;


// NOTE: this is sort of a singleton class.
class ImGuiRenderer
{
public:
  static void enableImGuiForWindow(GLFWwindow* window);

  explicit ImGuiRenderer(vk::Format target_format);

  void nextFrame();

  void render(
    vk::CommandBuffer cmd_buf,
    vk::Rect2D rect,
    vk::Image image,
    vk::ImageView image_view,
    ImDrawData* im_draw_data);

  ~ImGuiRenderer();

private:
  vk::UniqueDescriptorPool descriptorPool;
  ImGuiContext* context;

  void initImGui(vk::Format target_format);
  void cleanupImGui();
  void createDescriptorPool();
};
