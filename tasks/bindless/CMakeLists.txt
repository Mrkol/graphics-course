add_subdirectory(baker)

add_executable(bindless
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  baker/baker.cpp
)

target_include_directories(bindless PUBLIC baker)

target_link_libraries(bindless
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(bindless
  shaders/static_mesh.frag
  shaders/static_mesh.vert
)
