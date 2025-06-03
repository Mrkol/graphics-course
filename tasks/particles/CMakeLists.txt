add_executable(particles
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
)

target_include_directories(particles PUBLIC baker)

target_link_libraries(particles
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(particles
  shaders/static_mesh.frag
  shaders/static_mesh.vert
)
