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

void sokol_wake_up(void)
{
#if defined(__APPLE__)
    NSEvent* event = [NSEvent otherEventWithType: NSEventTypeApplicationDefined
                                                         location: NSMakePoint(0,0)
                                                   modifierFlags: 0
                                                       timestamp: 0.0
                                                    windowNumber: 0
                                                         context: nil
                                                         subtype: 0
                                                           data1: 0
                                                           data2: 0];
    [NSApp postEvent: event atStart: YES];
    
#elif defined(_WIN32)
    HWND hwnd = (HWND)sapp_win32_get_hwnd();
    PostMessage(hwnd, WM_USER + 1, 0, 0);
#elif defined(__linux__)
    XEvent evt;
    _sapp_clear(&evt, sizeof(evt));
    evt.xclient.type         = ClientMessage;
    evt.xclient.message_type = _sapp.x11.WM_STATE;
    evt.xclient.format       = 32;
    evt.xclient.window       = _sapp.x11.window;
    XSendEvent(_sapp.x11.display, _sapp.x11.window, false, NoEventMask, &evt);
    XFlush(_sapp.x11.display);
#else
#error "Unsupported platform"
#endif
}
