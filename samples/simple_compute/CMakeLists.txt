set(RENDER_SOURCE
  simple_compute.cpp
  compute_init.cpp
  execute.cpp)

add_executable(simple_compute main.cpp ${RENDER_SOURCE})
target_link_libraries(simple_compute PRIVATE glm::glm etna)
