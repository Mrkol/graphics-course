set(INSTANCING_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

add_executable(instancing
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  RockField.cpp
)
target_compile_definitions(instancing PRIVATE
  INSTANCING_ASSETS_ROOT="${INSTANCING_ASSETS_DIR}/"
)
target_link_libraries(instancing PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(instancing
  shaders/terrain.vert
  shaders/terrain.frag
  shaders/generate.comp
  shaders/rocks.vert
  shaders/rocks.frag
)
