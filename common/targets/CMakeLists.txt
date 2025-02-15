add_library(targets Backbuffer.cpp GBuffer.cpp Frame.cpp)

target_include_directories(targets PUBLIC ..)

target_link_libraries(targets PUBLIC etna glm::glm)

# Allows C++ code to inlcude C++-GLSL compat
# target_include_directories(targets INTERFACE shaders)
# Allow GLSL code to include helper files and compat
# target_shader_include_directories(targets INTERFACE shaders)

# target_add_shaders(targets
#   shaders/skybox.vert
#   shaders/skybox.frag
# )