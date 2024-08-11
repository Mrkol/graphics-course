
add_library(render_utils QuadRenderer.cpp)

target_include_directories(render_utils PUBLIC ..)

target_link_libraries(render_utils PUBLIC etna)
