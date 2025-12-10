
add_executable(local_shadertoy1
  main.cpp
  App.cpp
)

target_link_libraries(local_shadertoy1
  PRIVATE glfw etna glm::glm wsi gui)

target_add_shaders(local_shadertoy1
  shaders/toy.comp
)
