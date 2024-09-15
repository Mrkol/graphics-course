add_library(scene
  SceneLights.cpp SceneManager.cpp SceneMaterials.cpp SceneMeshes.cpp)

target_include_directories(scene PUBLIC ..)

target_link_libraries(scene PUBLIC glm::glm tinygltf etna render_utils)
