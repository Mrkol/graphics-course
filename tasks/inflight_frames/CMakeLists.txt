add_executable(inflight_frames
  main.cpp
  App.cpp
)

target_link_libraries(inflight_frames
  PRIVATE glfw etna glm::glm wsi gui Tracy::TracyClient)
target_link_libraries(inflight_frames
  PUBLIC render_utils scene)

target_add_shaders(inflight_frames
  shaders/toy.comp
  shaders/toy.vert
  shaders/toy.frag
  shaders/texture.frag
)

