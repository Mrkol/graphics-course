
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

# Manually add render_utils includes because no cmake 3.30
target_shader_include_directories(shadowmap PRIVATE "$<TARGET_PROPERTY:render_utils,INTERFACE_SHADER_INCLUDE_DIRECTORIES>")

target_add_shaders(shadowmap
  shaders/simple.vert
  shaders/simple_shadow.frag
)
