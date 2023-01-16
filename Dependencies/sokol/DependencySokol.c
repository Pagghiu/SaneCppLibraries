// Copyright (c) 2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "SCConfig.h"

#define SOKOL_APP_IMPL
#define SOKOL_IMPL
#include "DependencySokol.h"

static uint64_t gLastResetTime = 0;

#if defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)
static bool gShouldPauseEmscripten = false;
#endif


#if defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)

_SOKOL_PRIVATE EM_BOOL sapp_emsc_custom_frame(double time, void* userData)
{
    _SOKOL_UNUSED(userData);
    _sapp_timing_external(&_sapp.timing, time / 1000.0);

#if defined(SOKOL_WGPU)
    /*
        on WebGPU, the emscripten frame callback will already be called while
        the asynchronous WebGPU device and swapchain initialization is still
        in progress
    */
    switch (_sapp.emsc.wgpu.state)
    {
    case _SAPP_EMSC_WGPU_STATE_INITIAL:
        /* async JS init hasn't finished yet */
        break;
    case _SAPP_EMSC_WGPU_STATE_READY:
        /* perform post-async init stuff */
        _sapp_emsc_wgpu_surfaces_create();
        _sapp.emsc.wgpu.state = _SAPP_EMSC_WGPU_STATE_RUNNING;
        break;
    case _SAPP_EMSC_WGPU_STATE_RUNNING:
        /* a regular frame */
        _sapp_emsc_wgpu_next_frame();
        _sapp_frame();
        break;
    }
#else
    /* WebGL code path */
    _sapp_frame();
#endif
    /* quit-handling */
    if (_sapp.quit_requested)
    {
        _sapp_init_event(SAPP_EVENTTYPE_QUIT_REQUESTED);
        _sapp_call_event(&_sapp.event);
        if (_sapp.quit_requested)
        {
            _sapp.quit_ordered = true;
        }
    }
    if (_sapp.quit_ordered)
    {
        _sapp_emsc_unregister_eventhandlers();
        _sapp_call_cleanup();
        _sapp_discard_state();
        return EM_FALSE;
    }
    if (gShouldPauseEmscripten)
    {
        return EM_FALSE;
    }
    return EM_TRUE;
}
_SOKOL_PRIVATE void sapp_emsc_custom_run(const sapp_desc* desc)
{
    _sapp_init_state(desc);
    sapp_js_init(&_sapp.html5_canvas_selector[1]);
    double w, h;
    if (_sapp.desc.html5_canvas_resize)
    {
        w = (double)_sapp_def(_sapp.desc.width, _SAPP_FALLBACK_DEFAULT_WINDOW_WIDTH);
        h = (double)_sapp_def(_sapp.desc.height, _SAPP_FALLBACK_DEFAULT_WINDOW_HEIGHT);
    }
    else
    {
        emscripten_get_element_css_size(_sapp.html5_canvas_selector, &w, &h);
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, false, _sapp_emsc_size_changed);
    }
    if (_sapp.desc.high_dpi)
    {
        _sapp.dpi_scale = emscripten_get_device_pixel_ratio();
    }
    _sapp.window_width       = (int)roundf(w);
    _sapp.window_height      = (int)roundf(h);
    _sapp.framebuffer_width  = (int)roundf(w * _sapp.dpi_scale);
    _sapp.framebuffer_height = (int)roundf(h * _sapp.dpi_scale);
    emscripten_set_canvas_element_size(_sapp.html5_canvas_selector, _sapp.framebuffer_width, _sapp.framebuffer_height);
#if defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
    _sapp_emsc_webgl_init();
#elif defined(SOKOL_WGPU)
    sapp_js_wgpu_init();
#endif
    _sapp.valid = true;
    _sapp_emsc_register_eventhandlers();
    sapp_set_icon(&desc->icon);

    /* start the frame loop */
    emscripten_request_animation_frame_loop(sapp_emsc_custom_frame, 0);

    /* NOT A BUG: do not call _sapp_discard_state() here, instead this is
       called in sapp_emsc_custom_frame() when the application is ordered to quit
     */
}

int main(int argc, char* argv[])
{
    sapp_desc desc = sokol_get_desc(argc, argv);
    sapp_emsc_custom_run(&desc);
    return 0;
}
#else
sapp_desc sokol_main(int argc, char* argv[]) { return sokol_get_desc(argc, argv); }
#endif

void sokol_pause_rendering(void)
{
    if (gLastResetTime == 0)
    {
        gLastResetTime = stm_now();
    }

    if (stm_sec(stm_since(gLastResetTime)) > 0.5)
    {
#if defined(SOKOL_METAL)
#if TARGET_OS_OSX
        NSWindow* window  = (__bridge NSWindow*)sapp_macos_get_window();
        MTKView*  mtkView = window.contentView;
        mtkView.paused    = true;
#else
        UIWindow* window  = (__bridge UIWindow*)sapp_ios_get_window();
        MTKView*  mtkView = (MTKView*)window.rootViewController.view;
        mtkView.paused    = true;
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
#elif defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)
        gShouldPauseEmscripten = true;
#endif
        gLastResetTime = stm_now();
    }
}

#if defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)
_SOKOL_PRIVATE EM_BOOL sapp_emsc_custom_frame(double time, void* userData);
#endif
void sokol_unpause_rendering(void)
{
#if defined(SOKOL_METAL)
#if TARGET_OS_OSX
    NSWindow* window  = (__bridge NSWindow*)sapp_macos_get_window();
    MTKView*  mtkView = window.contentView;
    mtkView.paused    = false;
#else
    UIWindow* window  = (__bridge UIWindow*)sapp_ios_get_window();
    MTKView*  mtkView = (MTKView*)window.rootViewController.view;
    mtkView.paused    = false;
#endif
#elif defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)
    if (gShouldPauseEmscripten)
    {
        gShouldPauseEmscripten = false;
        emscripten_request_animation_frame_loop(sapp_emsc_custom_frame, 0);
    }
#endif
    gLastResetTime = stm_now();
}
