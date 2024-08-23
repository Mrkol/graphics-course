# To be included in subfolders where all targets are owned by us

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
  add_compile_options(/W4 /WX
    # Selectively disable some insane warnings
    /wd4061 /wd4514
    # Enforce standards-compliance in MSVC
    /permissive- /volatile:iso /Zc:inline /Zc:wchar_t /EHsc /Zc:preprocessor /Zc:__cplusplus
  )
else()
  add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

add_compile_definitions(
  GRAPHICS_COURSE_RESOURCES_ROOT="${PROJECT_SOURCE_DIR}/resources"
  GRAPHICS_COURSE_ROOT="${PROJECT_SOURCE_DIR}"
)
