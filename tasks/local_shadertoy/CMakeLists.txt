
add_executable(local_shadertoy
  main.cpp
  App.cpp
)

target_link_libraries(local_shadertoy
  PRIVATE glfw etna glm::glm wsi gui)

target_add_shaders(local_shadertoy
  shaders/toy.comp
)
