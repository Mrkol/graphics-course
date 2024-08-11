
add_library(gui ImGuiRenderer.cpp)

target_include_directories(gui PUBLIC ..)

target_link_libraries(gui PUBLIC DearImGui etna)
