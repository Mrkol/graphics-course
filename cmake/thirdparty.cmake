cmake_minimum_required(VERSION 3.20)


# Cross-platform WSI
CPMAddPackage(
  NAME glfw3
  GITHUB_REPOSITORY glfw/glfw
  GIT_TAG 3.4
  OPTIONS
    "GLFW_BUILD_TESTS OFF"
    "GLFW_BUILD_EXAMPLES OFF"
    "GLFW_BULID_DOCS OFF"
)

# Cross-platform 3D graphics
find_package(Vulkan 1.3.275 REQUIRED)

# Dear ImGui -- easiest way to do GUI
CPMAddPackage(
  NAME ImGui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.91.0
  DOWNLOAD_ONLY YES
)

if (ImGui_ADDED)
  add_library(DearImGui
    ${ImGui_SOURCE_DIR}/imgui.cpp ${ImGui_SOURCE_DIR}/imgui_draw.cpp
    ${ImGui_SOURCE_DIR}/imgui_tables.cpp ${ImGui_SOURCE_DIR}/imgui_widgets.cpp
    ${ImGui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp ${ImGui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)

  target_include_directories(DearImGui PUBLIC ${ImGui_SOURCE_DIR})

  target_link_libraries(DearImGui Vulkan::Vulkan)
  target_link_libraries(DearImGui glfw)
  target_compile_definitions(DearImGui PUBLIC IMGUI_USER_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/common/gui/ImGuiConfig.hpp")
endif ()

# Vector maths for graphics
CPMAddPackage("gh:g-truc/glm#master")

# glTF model parser
CPMAddPackage(
  NAME tinygltf
  GITHUB_REPOSITORY syoyo/tinygltf
  GIT_TAG v2.9.2
  OPTIONS
    "TINYGLTF_HEADER_ONLY OFF"
    "TINYGLTF_BUILD_LOADER_EXAMPLE OFF"
    "TINYGLTF_INSTALL OFF"
)

# etna -- our wrapper around Vulkan to make life easier
CPMAddPackage(
  NAME etna
  GITHUB_REPOSITORY AlexandrShcherbakov/etna
  VERSION 1.7.0
)

# Type-erased function containers that actually work
CPMAddPackage(
  GITHUB_REPOSITORY Naios/function2
  GIT_TAG 4.2.4
)
