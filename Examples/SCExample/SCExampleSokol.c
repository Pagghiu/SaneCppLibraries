// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#define SOKOL_APP_IMPL
#define SOKOL_IMPL

#include "SCExampleSokol.h"

void sokol_sleep(void)
{
#if defined(__APPLE__)
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, DBL_MAX, 1);
#elif defined(_WIN32)
    HWND hwnd = (HWND)sapp_win32_get_hwnd();
    MSG  msg;
    GetMessageW(&msg, NULL, 0, 0);
    if (msg.message == WM_QUIT)
    {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    else if (msg.message != WM_TIMER)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
#elif defined(__linux__)
    XEvent event;
    XNextEvent(_sapp.x11.display, &event);
    XPutBackEvent(_sapp.x11.display, &event);
#else
#error "Unsupported platform"
#endif
}
