
add_executable(simple_compute
  main.cpp
  simple_compute.cpp
  compute_init.cpp
  execute.cpp
)

target_link_libraries(simple_compute PRIVATE glm::glm etna)

target_add_shaders(simple_compute shaders/simple.comp)
