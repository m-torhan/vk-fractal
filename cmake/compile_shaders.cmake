# Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
#
# SPDX-License-Identifier: Apache-2.0

function(compile_glsl_shaders target)
  cmake_parse_arguments(ARG "" "OUT_DIR" "SOURCES" ${ARGN})
  if (NOT ARG_OUT_DIR)
    message(FATAL_ERROR "compile_glsl_shaders requires OUT_DIR")
  endif()

  find_program(GLSLC glslc REQUIRED)

  file(MAKE_DIRECTORY "${ARG_OUT_DIR}")

  set(OUT_SPV_FILES "")
  foreach(SHADER ${ARG_SOURCES})
    get_filename_component(NAME ${SHADER} NAME)
    set(OUT_SPV "${ARG_OUT_DIR}/${NAME}.spv")

    add_custom_command(
      OUTPUT "${OUT_SPV}"
      COMMAND "${GLSLC}"
        -O
        -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        -o "${OUT_SPV}"
        "${SHADER}"
      DEPENDS "${SHADER}"
      VERBATIM
    )
    list(APPEND OUT_SPV_FILES "${OUT_SPV}")
  endforeach()

  add_custom_target(${target}_shaders ALL DEPENDS ${OUT_SPV_FILES})
  add_dependencies(${target} ${target}_shaders)
endfunction()
