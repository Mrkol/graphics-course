add_library(boilerplate_pipeline Boilerplate.cpp)

target_include_directories(boilerplate_pipeline PUBLIC ../..)

# Allows C++ code to inlcude C++-GLSL compat
target_include_directories(boilerplate_pipeline INTERFACE shaders)
# Allow GLSL code to include helper files and compat
target_shader_include_directories(boilerplate_pipeline INTERFACE shaders)

target_link_libraries(boilerplate_pipeline PUBLIC etna targets gui PRIVATE scene)


target_add_shaders(boilerplate_pipeline
  shaders/boilerplate.vert
  shaders/boilerplate.frag
)