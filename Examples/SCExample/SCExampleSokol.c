// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#define SOKOL_APP_IMPL
#define SOKOL_IMPL

#include "SCExampleSokol.h"


void sokol_sleep(void)
{
#if defined(__APPLE__)
#if TARGET_OS_IPHONE
    
    _sapp.ios.view.paused = YES;
    // Not sure why we need 6 run loop to "eat" the pause event, probably they're used by MTKView
    for(int i =0 ; i < 6; ++i)
    {
        CFRunLoopRunResult res = CFRunLoopRunInMode(kCFRunLoopDefaultMode, DBL_MAX, YES);
        if(res == kCFRunLoopRunStopped)
        {
            break;
        }
    }
    _sapp.ios.view.paused = NO;
#else
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, DBL_MAX, 1);
#endif
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
#if TARGET_OS_IPHONE

    CFRunLoopStop(CFRunLoopGetMain());
#else
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
#endif
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
