
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

  foreach(glsl_path ${ARGN})
    set(input_path "${CMAKE_CURRENT_LIST_DIR}/${glsl_path}")
    set(output_path "${shader_binaries_dir}/$<PATH:GET_FILENAME,${glsl_path}>.spv")
    add_custom_command(
        OUTPUT ${output_path}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${shader_binaries_dir}
        COMMAND ${glslang_validator}
          "$<$<BOOL:${incl_dirs}>:-I$<JOIN:${incl_dirs},;-I>>"
          "$<$<CONFIG:Debug>:-g>"
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
  endforeach(glsl_path)

  set(custom_target_name "${tgt}_shaders")

  if(TARGET ${custom_target_name})
    message(FATAL_ERROR "Sorry, you can't call target_add_shaders multiple times cuz it's unimplemented. Fell free to create a PR.")
  else()
    set_target_properties(${tgt} PROPERTIES
      TRANSITIVE_COMPILE_PROPERTIES "SHADER_INCLUDE_DIRECTORIES"
    )

    add_custom_target(${custom_target_name} DEPENDS ${SPIRV_BINARY_FILES})
    add_dependencies(${tgt} ${custom_target_name})
    add_compile_definitions(${tgt}
      PRIVATE $<UPPER_CASE:${tgt}>_SHADERS_ROOT="${shader_binaries_dir}")
  endif()
endfunction()
