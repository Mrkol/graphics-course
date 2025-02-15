add_library(tonemap_pipeline Tonemap.cpp)

target_include_directories(tonemap_pipeline PUBLIC ../..)

# Allows C++ code to inlcude C++-GLSL compat
target_include_directories(tonemap_pipeline INTERFACE shaders)
# Allow GLSL code to include helper files and compat
target_shader_include_directories(tonemap_pipeline INTERFACE shaders)

target_link_libraries(tonemap_pipeline PUBLIC etna targets gui PRIVATE scene)


target_add_shaders(tonemap_pipeline
  shaders/tonemap.vert
  shaders/tonemap.frag
  shaders/distribution.comp
  shaders/histogram.comp
)