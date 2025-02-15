add_library(resolvegbuffer_pipeline ResolveGBuffer.cpp)

target_include_directories(resolvegbuffer_pipeline PUBLIC ../..)

# Allows C++ code to inlcude C++-GLSL compat
target_include_directories(resolvegbuffer_pipeline INTERFACE shaders)
# Allow GLSL code to include helper files and compat
target_shader_include_directories(resolvegbuffer_pipeline INTERFACE shaders)

target_link_libraries(resolvegbuffer_pipeline PUBLIC etna targets gui PRIVATE scene)


target_add_shaders(resolvegbuffer_pipeline
  shaders/deferred.vert
  shaders/deferred.frag
  shaders/sphere.vert
  shaders/sphere.frag
  shaders/sphere_deferred.frag
)