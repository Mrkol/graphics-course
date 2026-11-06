
# NOTE: We cannot do transitive properties before cmake 3.30, sadly
define_property(TARGET PROPERTY SHADER_INCLUDE_DIRECTORIES
  BRIEF_DOCS "Include directories for shader compilation"
  FULL_DOCS "Adds these include directories to all shaders of this target"
)

# But we can manually define and grab interface properties
define_property(TARGET PROPERTY INTERFACE_SHADER_INCLUDE_DIRECTORIES
  BRIEF_DOCS "Include directories for shader compilation"
  FULL_DOCS "Adds this include directories to all shaders of targets that depend on this one"
)

find_program(glslang_validator glslangValidator)
find_program(dxc_compiler dxc)

# Wokrs same way as target_include_directories, i.e. PUBLIC/PRIVATE/INTERFACE are supported
function(target_shader_include_directories tgt)
  list(POP_FRONT ${ARGN})
  set(current_type "PUBLIC")
  foreach(arg ${ARGN})
    if("${arg}" STREQUAL "INTERFACE" OR "${arg}" STREQUAL "PUBLIC" OR "${arg}" STREQUAL "PRIVATE")
      set(current_type "${arg}")
      continue()
    endif()

    set(abs_path "$<PATH:ABSOLUTE_PATH,NORMALIZE,$<TARGET_GENEX_EVAL:${tgt},${arg}>,$<TARGET_PROPERTY:${tgt},SOURCE_DIR>>")

    if(${current_type} STREQUAL "PUBLIC")
      set_property(TARGET ${tgt} APPEND PROPERTY SHADER_INCLUDE_DIRECTORIES ${abs_path})
      set_property(TARGET ${tgt} APPEND PROPERTY INTERFACE_SHADER_INCLUDE_DIRECTORIES ${abs_path})
    elseif(${current_type} STREQUAL "PRIVATE")
      set_property(TARGET ${tgt} APPEND PROPERTY SHADER_INCLUDE_DIRECTORIES ${abs_path})
    elseif(${current_type} STREQUAL "INTERFACE")
      set_property(TARGET ${tgt} APPEND PROPERTY INTERFACE_SHADER_INCLUDE_DIRECTORIES ${abs_path})
    endif()
  endforeach(arg)
endfunction()

function(target_add_shaders tgt)
  list(POP_FRONT ${ARGN})

  set(shader_binaries_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

  set(incl_dirs "$<TARGET_GENEX_EVAL:${tgt},$<TARGET_PROPERTY:${tgt},SHADER_INCLUDE_DIRECTORIES>>")

  foreach(shader_path ${ARGN})
    set(input_path "${CMAKE_CURRENT_LIST_DIR}/${shader_path}")
    set(output_path "${shader_binaries_dir}/$<PATH:GET_FILENAME,${shader_path}>.spv")
    add_custom_command(
        OUTPUT ${output_path}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_binaries_dir}
        COMMAND ${glslang_validator}
          "$<$<BOOL:${incl_dirs}>:-I$<JOIN:${incl_dirs},;-I>>"
          "$<$<CONFIG:Debug>:-g>"
          -D__glsl
          -V
          ${input_path}
          -o ${output_path}
          --depfile "${output_path}.d"
        VERBATIM
        COMMAND_EXPAND_LISTS
        DEPENDS ${input_path}
        DEPFILE "${output_path}.d"
      )
    list(APPEND SPIRV_BINARY_FILES ${output_path})
  endforeach(shader_path)

  _target_add_shader_target(${tgt} glsl "${shader_binaries_dir}" ${SPIRV_BINARY_FILES})
endfunction()

function(target_add_shaders_hlsl tgt)
  list(POP_FRONT ${ARGN})

  set(shader_binaries_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders/")

  set(incl_dirs "$<TARGET_GENEX_EVAL:${tgt},$<TARGET_PROPERTY:${tgt},SHADER_INCLUDE_DIRECTORIES>>")

  # A shader can optionally be followed by SHADER_MODEL <major_minor>.
  # For example: shaders/simple.frag.hlsl SHADER_MODEL 6_6
  set(shader_args ${ARGN})
  while(shader_args)
    list(POP_FRONT shader_args shader_path)
    set(shader_model 6_0)

    if(shader_args)
      list(GET shader_args 0 next_arg)
      if(next_arg STREQUAL "SHADER_MODEL")
        list(POP_FRONT shader_args)
        if(NOT shader_args)
          message(FATAL_ERROR "SHADER_MODEL must be followed by a shader model version.")
        endif()
        list(POP_FRONT shader_args shader_model)
        if(NOT shader_model MATCHES "^[0-9]+_[0-9]+$")
          message(FATAL_ERROR "Invalid HLSL shader model '${shader_model}'. Use <major>_<minor>, for example 6_6.")
        endif()
      endif()
    endif()

    if(shader_path MATCHES "\\.vert\\.hlsl$")
      set(shader_stage vs)
    elseif(shader_path MATCHES "\\.frag\\.hlsl$")
      set(shader_stage ps)
    elseif(shader_path MATCHES "\\.comp\\.hlsl$")
      set(shader_stage cs)
    else()
      message(FATAL_ERROR "Cannot infer a DXC profile from HLSL shader '${shader_path}'.")
    endif()
    set(shader_profile ${shader_stage}_${shader_model})

    set(input_path "${CMAKE_CURRENT_LIST_DIR}/${shader_path}")
    set(output_path "${shader_binaries_dir}/$<PATH:GET_FILENAME,${shader_path}>.spv")
    add_custom_command(
        OUTPUT ${output_path}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_binaries_dir}
        COMMAND ${dxc_compiler}
          "$<$<BOOL:${incl_dirs}>:-I$<JOIN:${incl_dirs},;-I>>"
          "$<$<CONFIG:Debug>:-Zi>"
          -D__hlsl
          -E main
          -T ${shader_profile}
          -spirv
          -fspv-target-env=vulkan1.3
          ${input_path}
          -Fo ${output_path}
        VERBATIM
        COMMAND_EXPAND_LISTS
        DEPENDS ${input_path}
      )
    list(APPEND SPIRV_BINARY_FILES ${output_path})
  endwhile()

  _target_add_shader_target(${tgt} hlsl "${shader_binaries_dir}" ${SPIRV_BINARY_FILES})
endfunction()

function(_target_add_shader_target tgt shader_type shader_binaries_dir)
  set(custom_target_name "${tgt}_${shader_type}_shaders")

  if(TARGET ${custom_target_name})
    message(FATAL_ERROR "Sorry, you can't call target_add_shaders_${shader_type} multiple times cuz it's unimplemented. Fell free to create a PR.")
  endif()

  if(NOT TARGET ${tgt}_shaders)
    set_target_properties(${tgt} PROPERTIES
      TRANSITIVE_COMPILE_PROPERTIES "SHADER_INCLUDE_DIRECTORIES"
    )

    add_custom_target(${tgt}_shaders)
    add_dependencies(${tgt} ${tgt}_shaders)
    target_compile_definitions(${tgt}
      PRIVATE $<UPPER_CASE:${tgt}>_SHADERS_ROOT="${shader_binaries_dir}")
  endif()

  add_custom_target(${custom_target_name} DEPENDS ${ARGN})
  add_dependencies(${tgt}_shaders ${custom_target_name})
endfunction()
