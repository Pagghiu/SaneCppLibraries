// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#if defined(__APPLE__)
#define SOKOL_METAL
#elif defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__linux__)
#define SOKOL_GLES3
#else
#error "Unsupported platform"
#endif

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#elif __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include "sokol_app.h"

#if __clang__
#pragma clang diagnostic pop
#elif __GNUC__
#pragma GCC diagnostic pop
#endif

#include "sokol_gfx.h"
#include "sokol_glue.h"
#ifdef __cplusplus
extern "C"
{
#endif

    void sokol_sleep(void);

#ifdef __cplusplus
}
#endif
