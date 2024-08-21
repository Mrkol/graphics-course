
add_library(wsi OsWindow.cpp OsWindowingManager.cpp)

target_include_directories(wsi PUBLIC ..)

target_link_libraries(wsi PUBLIC glm::glm function2::function2)
target_link_libraries(wsi PRIVATE glfw etna Tracy::TracyClient)
