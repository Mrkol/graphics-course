
add_executable(imgui_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  Perlin.cpp
)

target_link_libraries(imgui_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(imgui_renderer
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
  shaders/deferred.vert
  shaders/deferred.frag
  shaders/sphere.vert
  shaders/sphere.frag
  shaders/skybox.vert
  shaders/skybox.frag
  shaders/sphere_deferred.frag
)
