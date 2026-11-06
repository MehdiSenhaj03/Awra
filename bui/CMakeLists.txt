# Awra's fork of BUI (https://github.com/ppmpreetham/BUI, upstream 79be8e3).
#
# Only the runtime layers are kept: core/ (blenlib subset), types/ (headers),
# gpu/ (software raster, text, optional GPU renderers) and kit/ (node tree,
# input, damage rendering, images, native window hosts). Awra adds the
# Wayland host, premultiplied-alpha frames, translucent native windows with
# compositor blur and the class registration API its widgets build on.
# See FORK.md for every change from upstream.
#
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.20)
project(bui LANGUAGES C CXX VERSION 0.4.1)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(${CMAKE_CURRENT_SOURCE_DIR}/features.cmake)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options($<$<CONFIG:Release>:/O2> /Gy /Gw /W3 /utf-8)
  add_compile_definitions(_USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-ffunction-sections -fdata-sections -Wall -Wno-unused-parameter
                      -Wno-missing-field-initializers)
endif()

set(BUI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
# Every layer below resolves ${CMAKE_SOURCE_DIR}/... paths of upstream's tree:
# keep them working when this directory is added from another project.
set(BUI_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(core)
add_subdirectory(types)
add_subdirectory(gpu)
add_subdirectory(kit)
