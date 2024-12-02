add_executable(local_shadertoy2
  main.cpp
  App.cpp
)

target_link_libraries(local_shadertoy2
  PRIVATE glfw etna glm::glm wsi gui)

target_add_shaders(local_shadertoy2
  shaders/toy.vert
  shaders/toy.frag
  shaders/texture.frag
)

target_link_libraries(local_shadertoy2 PUBLIC render_utils scene)

add_compile_definitions(local_shadertoy2 PRIVATE TEXTURES_ROOT="${PROJECT_SOURCE_DIR}/resources/textures/")