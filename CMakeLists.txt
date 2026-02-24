# Copyright (c) 2026 Maciej Torhan <https://github.com/m-torhan>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)
project(vk_fractal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VK_FRACTAL_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# --- Dependencies ---
find_package(Vulkan REQUIRED)

include(FetchContent)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.9
)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG        v1.92.6
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp

  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui
  PUBLIC
    glfw
    Vulkan::Vulkan
)

# --- Shader compilation helper ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(compile_shaders)

# --- Target ---
add_executable(vk_fractal
  src/main.cpp
  src/app/app.hpp src/app/app.cpp
  src/gfx/camera.hpp src/gfx/camera.cpp
  src/gfx/imgui_layer.hpp src/gfx/imgui_layer.cpp
  src/gfx/vk_bootstrap.hpp
  src/gfx/vk_context.hpp src/gfx/vk_context.cpp
  src/gfx/swapchain.hpp src/gfx/swapchain.cpp
  src/gfx/fullscreen_pipeline.hpp src/gfx/fullscreen_pipeline.cpp
  src/gfx/frame_resources.hpp src/gfx/frame_resources.cpp
  src/util/checks.hpp src/util/checks.cpp
  src/util/read_file.hpp src/util/read_file.cpp
)

target_include_directories(vk_fractal PRIVATE src)
target_link_libraries(vk_fractal PRIVATE Vulkan::Vulkan glfw glm::glm imgui)

target_compile_options(vk_fractal PRIVATE -Wall -Wextra -Wpedantic -Wno-missing-field-initializers)

# --- Shaders ---
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

compile_glsl_shaders(vk_fractal
  OUT_DIR "${SHADER_OUT_DIR}"
  SOURCES
    "${SHADER_SRC_DIR}/fullscreen.vert"
    "${SHADER_SRC_DIR}/fullscreen.frag"
)

# Make runtime find shaders easily
add_custom_command(TARGET vk_fractal POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:vk_fractal>/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADER_OUT_DIR}" "$<TARGET_FILE_DIR:vk_fractal>/shaders"
)
