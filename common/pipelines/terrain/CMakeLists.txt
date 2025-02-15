add_library(terrain_pipeline Terrain.cpp)

target_include_directories(terrain_pipeline PUBLIC ../..)

# Allows C++ code to inlcude C++-GLSL compat
target_include_directories(terrain_pipeline INTERFACE shaders)
# Allow GLSL code to include helper files and compat
target_shader_include_directories(terrain_pipeline INTERFACE shaders)

target_link_libraries(terrain_pipeline PUBLIC etna targets gui PRIVATE scene perlin_pipeline)


target_add_shaders(terrain_pipeline
  shaders/terrain.vert
  shaders/terrain.frag
  shaders/terrain.tese
  shaders/terrain.tesc
)