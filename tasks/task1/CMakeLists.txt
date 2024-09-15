
add_executable(task1
  main.cpp
  Renderer.cpp
  WorldRenderer.cpp
  App.cpp
)

target_link_libraries(task1
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(task1
  shaders/g_buffer_pass.vert
  shaders/g_buffer_pass.frag
  shaders/finite_point_light_pass.vert
  shaders/finite_point_light_pass.frag
  shaders/infinite_point_light_pass.vert
  shaders/infinite_point_light_pass.frag
  shaders/directional_light_pass.vert
  shaders/directional_light_pass.frag
  shaders/ambient_light_pass.vert
  shaders/ambient_light_pass.frag
  shaders/emissive_light_pass.vert
  shaders/emissive_light_pass.frag
)

target_shader_include_directories(task1 ../../common/scene/light)
