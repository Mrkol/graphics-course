
add_executable(pbr_baker
  main.cpp
)

target_link_libraries(pbr_baker
  PRIVATE tinygltf glm::glm etna)
