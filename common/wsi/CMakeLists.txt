
add_library(wsi OsWindow.cpp OsWindowingManager.cpp)

target_include_directories(wsi PUBLIC ..)

target_link_libraries(wsi PUBLIC glm::glm)
target_link_libraries(wsi PRIVATE glfw etna)
