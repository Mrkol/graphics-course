cmake_minimum_required(VERSION 3.30)

project(graphics_course CXX)

# Prevent the user from polluting the root directory with build files
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

include("cmake/get_cpm.cmake")

# NOTE: uncomment to run with ASAN (works even on windows!)
# Also note that I couldn't figure out how to make ASAN work on windows with clang & ldd.
# We intentially set this stuff here, as it must apply globally, to all targets,
# including our dependencies
if(false)
  if (MSVC)
    add_compile_options("/fsanitize=address")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
  else()
    add_compile_options("/fsanitize=address")
  endif()
endif()

# Uncomment to contribute to etna
# set(CPM_etna_SOURCE "${PROJECT_SOURCE_DIR}/../etna")

include("cmake/thirdparty.cmake")
include("cmake/shaders.cmake")

add_subdirectory(common)
add_subdirectory(samples)
add_subdirectory(tasks)
