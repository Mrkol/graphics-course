
add_executable(imgui_baker
  main.cpp baker.cpp
)

target_link_libraries(imgui_baker
  PRIVATE tinygltf)
