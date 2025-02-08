
add_executable(tonemapping_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  Perlin.cpp
)

target_link_libraries(tonemapping_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(tonemapping_renderer
  shaders/static_mesh.frag
  shaders/static_mesh.vert
  shaders/quad.vert
  shaders/perlin.frag
  shaders/terrain.vert
  shaders/terrain.frag
  shaders/terrain.tesc
  shaders/terrain.tese
  shaders/postprocess.frag
  shaders/histogram.comp
  shaders/distribution.comp
)
