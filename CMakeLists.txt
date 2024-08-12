cmake_minimum_required(VERSION 3.20)

project(graphics_course CXX)

# Prevent the user from polluting the root directory with build files
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

include("cmake/get_cpm.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# NOTE: uncomment to run with ASAN (works even on windows!)
# Also note that I couldn't figure out how to make ASAN work on windows with clang & ldd.
if (false)
  if (MSVC)
    add_compile_options("/fsanitize=address")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  else()
    add_compile_options("/fsanitize=address")
  endif()
endif()

include("cmake/thirdparty.cmake")
include("cmake/shaders.cmake")


if (MSVC)
  add_compile_options(/W3)
else()
  add_compile_options(-Wall -Wextra)
endif()

add_compile_definitions(
  GRAPHICS_COURSE_RESOURCES_ROOT="${PROJECT_SOURCE_DIR}/resources"
  GRAPHICS_COURSE_ROOT="${PROJECT_SOURCE_DIR}"
)

add_subdirectory(../etna etna)

add_subdirectory(common)
add_subdirectory(samples)
