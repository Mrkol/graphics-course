add_library(staticmesh_pipeline StaticMesh.cpp)

target_include_directories(staticmesh_pipeline PUBLIC ../..)

# Allows C++ code to inlcude C++-GLSL compat
target_include_directories(staticmesh_pipeline INTERFACE shaders)
# Allow GLSL code to include helper files and compat
target_shader_include_directories(staticmesh_pipeline INTERFACE shaders)

target_link_libraries(staticmesh_pipeline PUBLIC etna targets gui PRIVATE scene render_utils)


target_add_shaders(staticmesh_pipeline
  shaders/staticmesh.vert
  shaders/staticmesh.frag
)