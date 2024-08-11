
add_executable(shadowmap
  main.cpp
  Renderer.cpp
  render_init.cpp
  update.cpp
  draw.cpp
  gui.cpp
  App.cpp
)

target_link_libraries(shadowmap PRIVATE glfw etna glm::glm
  wsi gui scene render_utils)
