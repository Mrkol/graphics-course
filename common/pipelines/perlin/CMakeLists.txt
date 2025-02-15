add_library(perlin_pipeline Perlin.cpp)

target_include_directories(perlin_pipeline PUBLIC ../..)

# Allows C++ code to inlcude C++-GLSL compat
target_include_directories(perlin_pipeline INTERFACE shaders)
# Allow GLSL code to include helper files and compat
target_shader_include_directories(perlin_pipeline INTERFACE shaders)

target_link_libraries(perlin_pipeline PUBLIC etna targets gui PRIVATE scene)


target_add_shaders(perlin_pipeline
  shaders/perlin.vert
  shaders/perlin.frag
)