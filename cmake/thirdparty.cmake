cmake_minimum_required(VERSION 3.30)


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

# Cross-platform 3D graphics (loader from system/SDK)
find_package(Vulkan 1.4.300 REQUIRED)
# C++ API in system packages is often stale; use Khronos headers matching the course.
CPMAddPackage(
  NAME VulkanHeaders
  GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers
  GIT_TAG vulkan-sdk-1.4.304.0
  DOWNLOAD_ONLY YES
)
if(VulkanHeaders_ADDED AND TARGET Vulkan::Vulkan)
  set_target_properties(Vulkan::Vulkan PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${VulkanHeaders_SOURCE_DIR}/include")
endif()

# Dear ImGui -- easiest way to do GUI
CPMAddPackage(
  NAME ImGui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.91.8
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
# Upstream etna requires CMake 3.25; patch allows 3.22 (Ubuntu 22.04 default).
CPMAddPackage(
  NAME etna
  GITHUB_REPOSITORY AlexandrShcherbakov/etna
  VERSION 1.12.0
  PATCHES
    "${CMAKE_CURRENT_LIST_DIR}/patches/etna-cmake-3.22.patch"
    "${CMAKE_CURRENT_LIST_DIR}/patches/etna-gcc11-state-tracking.patch"
)

# Type-erased function containers that actually work
CPMAddPackage(
  GITHUB_REPOSITORY Naios/function2
  GIT_TAG 4.2.4
)
