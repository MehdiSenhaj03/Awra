# Awra 2: Awra's frosted-glass UI framework on BUI (bui/, Awra's fork of
# Blender's interface runtime). One build for Linux (Wayland, X11) and
# Windows (MinGW or MSVC); Rust uses it through bindings/rust/awra-sys.
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)
project(awra VERSION 2.0.0 LANGUAGES C CXX)

option(AWRA_SHOWCASE "Build the showcase and the screenshot tool" ON)
option(AWRA_TESTS "Build the tests" ON)
option(AWRA_EMBED_FONT "Embed Inter as a fallback face (+800 KB)" OFF)
option(AWRA_GPU "Build BUI's GPU renderers (Direct3D 11 / OpenGL)" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# BUI's optional parts: what Awra uses, nothing more (each costs binary size)
set(BUI_VIDEO OFF CACHE BOOL "" FORCE)
set(BUI_BUILD_IMGUI OFF CACHE BOOL "" FORCE)
set(BUI_FILEBROWSER OFF CACHE BOOL "" FORCE)
set(BUI_GPU ${AWRA_GPU} CACHE BOOL "" FORCE)
set(BUI_IMAGE_BMP OFF CACHE BOOL "" FORCE)
set(BUI_IMAGE_TGA OFF CACHE BOOL "" FORCE)
set(BUI_IMAGE_QOI OFF CACHE BOOL "" FORCE)
set(BUI_IMAGE_HDR OFF CACHE BOOL "" FORCE)
set(BUI_IMAGE_PNM OFF CACHE BOOL "" FORCE)
# MinGW: the runtime (libgcc, libstdc++, winpthread) linked in, so the
# executables need no DLL beside them
if(MINGW)
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

add_subdirectory(bui)
if(NOT AWRA_EMBED_FONT)
  target_compile_definitions(bui_gpu PRIVATE BUI_NO_INTER)
endif()

file(GLOB AWRA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
add_library(awra STATIC ${AWRA_SOURCES})
set_target_properties(awra PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_include_directories(awra
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/bui/kit/intern)
target_link_libraries(awra PUBLIC bui_kit)
if(MSVC)
  target_compile_options(awra PRIVATE /W3)
else()
  target_compile_options(awra PRIVATE -Wall -Wno-unused-parameter -Wno-missing-field-initializers
                                      -ffunction-sections -fdata-sections)
endif()
if(WIN32)
  target_link_libraries(awra PUBLIC dwmapi uxtheme shell32 ole32 uuid comdlg32)
endif()

if(AWRA_SHOWCASE)
  add_executable(awra-showcase showcase/showcase.c)
  target_link_libraries(awra-showcase PRIVATE awra)
  add_executable(awra-shot tools/awra-shot.c showcase/pages.c)
  target_link_libraries(awra-shot PRIVATE awra)
  target_compile_definitions(awra-shot PRIVATE AWRA_SHOT_ONLY)
  target_sources(awra-showcase PRIVATE showcase/pages.c)
endif()
