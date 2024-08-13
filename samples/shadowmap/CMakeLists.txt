
add_executable(shadowmap
  main.cpp
  Renderer.cpp
  WorldRenderer.cpp
  App.cpp
)

target_link_libraries(shadowmap
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(shadowmap
  shaders/simple.vert
  shaders/simple_shadow.frag
)
