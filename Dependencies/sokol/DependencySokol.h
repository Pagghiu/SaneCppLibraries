// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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

#include "_sokol/sokol_app.h"
#include "_sokol/sokol_gfx.h"
#include "_sokol/sokol_glue.h"
#include "_sokol/sokol_time.h"
#include "_sokol/sokol_fetch.h"
#ifdef __cplusplus
extern "C" {
#endif

void sokol_pause_rendering(void);
void sokol_unpause_rendering(void);
sapp_desc sokol_get_desc(int argc, char* argv[]);
#ifdef __cplusplus
}
#endif
