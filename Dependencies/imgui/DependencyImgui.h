#pragma once
#if SC_ENABLE_FREETYPE
#define IMGUI_USE_WCHAR32 1
#endif
#include "_imgui/imgui.h"
#if SC_ENABLE_FREETYPE
#include "_imgui/misc/freetype/imgui_freetype.h"
#endif

void sokol_delay_init_imgui(void);
