
add_executable(many_objects_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
)

target_link_libraries(many_objects_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(many_objects_renderer
  shaders/static_mesh.frag
  shaders/static_mesh.vert
)
