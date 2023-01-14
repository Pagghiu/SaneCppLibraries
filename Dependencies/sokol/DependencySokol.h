#pragma once

#if defined(__APPLE__)
#define SOKOL_METAL
#elif defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__linux__)
#define SOKOL_GLCORE33
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES2
#define SOKOL_NO_ENTRY
#endif

#include "../imgui/DependencyImgui.h"

#include "_sokol/sokol_app.h"
#include "_sokol/sokol_gfx.h"
#include "_sokol/sokol_glue.h"
#include "_sokol/sokol_time.h"
#include "_sokol/sokol_fetch.h"
#include "_sokol/util/sokol_imgui.h"

void sokol_delay_init_imgui();
void sokol_pause_rendering();
void sokol_unpause_rendering();
sapp_desc sokol_get_desc(int argc, char* argv[]);
