
add_library(render_utils QuadRenderer.cpp)

target_include_directories(render_utils PUBLIC ..)

# Allows C++ code to inlcude C++-GLSL compat
target_include_directories(render_utils INTERFACE shaders)
# Allow GLSL code to include helper files and compat
target_shader_include_directories(render_utils INTERFACE shaders)

target_link_libraries(render_utils PUBLIC etna)


target_add_shaders(render_utils
  shaders/quad.vert
  shaders/quad.frag
)
