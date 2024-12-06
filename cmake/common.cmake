# To be included in subfolders where all targets are owned by us

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC") # cl and clang-cl
  add_compile_options(/W4 /WX
    # Selectively disable some insane warnings
    /wd4061 /wd4514 /wd4324
    # Enforce standards-compliance in MSVC
    /permissive- /volatile:iso /Zc:inline /Zc:wchar_t /EHsc /Zc:__cplusplus
  )
  if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC") # cl only
    add_compile_options(/Zc:preprocessor)
  endif()
elseif(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU") # gcc and clang
  add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

add_compile_definitions(
  GRAPHICS_COURSE_RESOURCES_ROOT="${PROJECT_SOURCE_DIR}/resources"
  GRAPHICS_COURSE_ROOT="${PROJECT_SOURCE_DIR}"
)
