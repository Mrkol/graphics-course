
add_executable(inflight_frames
  App.cpp
)

add_compile_options("/fsanitize=address -Wall -Wextra -Werror -Wpedantic")

target_link_libraries(inflight_frames
  PRIVATE glfw etna glm::glm wsi gui render_utils scene)

target_add_shaders(inflight_frames
  shaders/toy.frag
  shaders/toy.vert
  shaders/texture.frag
)