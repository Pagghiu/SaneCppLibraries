#include "SCConfig.h"

// clang-format off
#if SC_ENABLE_FREETYPE
#define IMGUI_ENABLE_FREETYPE
#endif

#include "_imgui/imgui.cpp"
#include "_imgui/imgui_draw.cpp"
#include "_imgui/imgui_tables.cpp"
#include "_imgui/imgui_widgets.cpp"
#include "_imgui/imgui_demo.cpp"

#if SC_ENABLE_FREETYPE
#if _MSC_VER
#pragma warning (push)
#elif __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#elif __GCC__
#endif
#ifndef FT_CONFIG_CONFIG_H
#define FT_CONFIG_CONFIG_H  <freetype/../../../config/ftconfig.h>
#endif
#ifndef FT_CONFIG_OPTIONS_H
#define FT_CONFIG_OPTIONS_H  <freetype/../../../config/ftoption.h>
#endif

#include "_imgui/misc/freetype/imgui_freetype.cpp"

#if _MSC_VER
#pragma warning (pop)
#elif __clang__
#pragma clang diagnostic pop
#else
#endif
#endif // IMGUI_ENABLE_FREETYPE
// clang-format on
