# bui_gpu - drawing abstraction with a null backend
#
# The UI toolkit draws exclusively through these seam headers. Ship a real
# renderer by implementing the same API (GL / Metal / software rasterizer).
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(SRC
  intern/gpu_null.c
  intern/gpu_raster.c
  intern/gpu_d3d11.c # without BUI_GPU (features.json), the stubs that keep rasters on the CPU
)

add_library(bui_gpu STATIC ${SRC})
add_library(bui::gpu ALIAS bui_gpu)
bui_features(bui_gpu)

target_include_directories(bui_gpu PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${BUI_ROOT}/core/include
)
target_link_libraries(bui_gpu PUBLIC bui::core)
if(NOT MSVC)
  target_link_libraries(bui_gpu PRIVATE m)
endif()
# Text: the portable stb_truetype engine everywhere (Inter embedded as the
# default face, like Blender; glyph outlines feed the GPU's Slug renderer).
# BUI_TEXT_GDI=ON switches Windows to the native GDI engine (system fonts,
# ClearType-style hinting, no Slug). BUI_TEXT_STB is kept as a no-op.
option(BUI_TEXT_GDI "Use the native GDI text engine on Windows" OFF)
option(BUI_TEXT_STB "Deprecated: the portable engine is the default" ON)
# GPU text: Slug (glyph outlines in the pixel shader) or the per-glyph atlas.
# Runtime: BUI_gpu_set_slug() / BUI_SLUG=1|0; this sets the default.
option(BUI_SLUG "GPU renderer draws text with Slug by default (else a glyph atlas)" ON)
target_compile_definitions(bui_gpu PRIVATE BUI_SLUG_DEFAULT=$<BOOL:${BUI_SLUG}>)
if(BUI_TEXT_GDI)
  target_compile_definitions(bui_gpu PRIVATE BUI_TEXT_GDI=1)
endif()
# FreeType (extern/freetype, 2.13.3 as Blender uses): loads, hints and renders
# glyphs as GTK 4 does through cairo-ft (BUI-Awra: the fontconfig pattern's
# and the desktop's hinting, synthetic bold with ftsynth.c). Compiled into bui_gpu itself
# (BUI_TEXT_FREETYPE, features.json).
if(BUI_TEXT_FREETYPE AND NOT BUI_TEXT_GDI)
  set(FT ${CMAKE_CURRENT_SOURCE_DIR}/extern/freetype)
  set(FT_SRC
    ${FT}/src/base/ftsystem.c ${FT}/src/base/ftinit.c ${FT}/src/base/ftdebug.c
    ${FT}/src/base/ftbase.c ${FT}/src/base/ftmm.c ${FT}/src/base/ftbitmap.c ${FT}/src/base/ftbbox.c
    ${FT}/src/base/ftsynth.c
    ${FT}/src/truetype/truetype.c ${FT}/src/sfnt/sfnt.c ${FT}/src/autofit/autofit.c
    ${FT}/src/smooth/smooth.c ${FT}/src/psnames/psnames.c ${FT}/src/cff/cff.c
    ${FT}/src/psaux/psaux.c ${FT}/src/pshinter/pshinter.c
  )
  target_sources(bui_gpu PRIVATE ${FT_SRC})
  set_source_files_properties(${FT_SRC} PROPERTIES COMPILE_DEFINITIONS FT2_BUILD_LIBRARY)
  if(MSVC)
    set_source_files_properties(${FT_SRC} PROPERTIES COMPILE_OPTIONS "/w")
  else()
    set_source_files_properties(${FT_SRC} PROPERTIES COMPILE_OPTIONS "-w")
  endif()
  target_include_directories(bui_gpu PRIVATE ${FT}/bui ${FT}/include)
  target_compile_definitions(bui_gpu PRIVATE
    BUI_TEXT_FT=1
    "FT_CONFIG_OPTIONS_H=<bui_ftoption.h>"
    "FT_CONFIG_MODULES_H=<bui_ftmodule.h>"
  )
endif()
# BUI-Awra: bidi and shaping as Pango does them, the same on every platform:
# HarfBuzz (extern/harfbuzz, 14.6.0 as GTK 4.22 uses it: one translation
# unit, HB_LEAN plus what text needs, see src/hb-bui-config.h) and FriBidi
# (extern/fribidi, 1.0.16, its bidi algorithm only). Both are compiled into
# bui_gpu with hidden symbols, so a process that also loads the system's
# libraries keeps them apart.
set(HB ${CMAKE_CURRENT_SOURCE_DIR}/extern/harfbuzz/src)
set(FRIBIDI ${CMAKE_CURRENT_SOURCE_DIR}/extern/fribidi)
set(FRIBIDI_SRC
  ${FRIBIDI}/fribidi-bidi.c ${FRIBIDI}/fribidi-bidi-types.c ${FRIBIDI}/fribidi-brackets.c
  ${FRIBIDI}/fribidi-mirroring.c ${FRIBIDI}/fribidi-run.c
)
target_sources(bui_gpu PRIVATE ${HB}/hb-bui.cc ${FRIBIDI_SRC})
target_include_directories(bui_gpu PRIVATE ${HB} ${FRIBIDI})
target_compile_definitions(bui_gpu PRIVATE FRIBIDI_LIB_STATIC)
set_source_files_properties(${HB}/hb-bui.cc PROPERTIES COMPILE_DEFINITIONS
  "HB_LEAN;HB_CONFIG_OVERRIDE_H=\"hb-bui-config.h\"")
set_source_files_properties(${FRIBIDI_SRC} PROPERTIES COMPILE_DEFINITIONS
  "FRIBIDI_BUILD;HAVE_STRINGIZE=1;HAVE_STDLIB_H=1;HAVE_STRING_H=1;HAVE_MEMORY_H=1")
if(MSVC)
  set_source_files_properties(${HB}/hb-bui.cc PROPERTIES COMPILE_OPTIONS "/w;/GR-")
  set_source_files_properties(${FRIBIDI_SRC} PROPERTIES COMPILE_OPTIONS "/w")
else()
  set_source_files_properties(${HB}/hb-bui.cc PROPERTIES COMPILE_OPTIONS
    "-w;-std=c++11;-fno-exceptions;-fno-rtti;-fvisibility=hidden;-fvisibility-inlines-hidden")
  set_source_files_properties(${FRIBIDI_SRC} PROPERTIES COMPILE_OPTIONS "-w;-fvisibility=hidden")
endif()
if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(bui_gpu PRIVATE Threads::Threads)
endif()

if(WIN32)
  # gpu_raster.c: GDI glyph-run text engine + native window present.
  target_link_libraries(bui_gpu PUBLIC gdi32 user32 advapi32)
elseif(NOT APPLE AND BUI_GPU)
  # GPU renderer: OpenGL 3.3 (GLX windows, EGL offscreen) in gpu_gl.c instead
  # of Direct3D 11. libGL / libEGL / libX11 are dlopen()ed at run time, never
  # linked, so nothing GL is needed to build or import.
  target_sources(bui_gpu PRIVATE intern/gpu_gl.c)
  target_compile_definitions(bui_gpu PRIVATE BUI_GPU_GL=1)
  target_link_libraries(bui_gpu PRIVATE ${CMAKE_DL_LIBS})
endif()
