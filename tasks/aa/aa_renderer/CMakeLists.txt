add_executable(aa_renderer
  main.cpp
  Renderer.cpp
  WorldRenderer.cpp
  App.cpp
)

target_link_libraries(aa_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(aa_renderer
  shaders/simple.vert
  shaders/simple_shadow.frag
  shaders/fxaa.frag
  shaders/simple_fs.vert
)
