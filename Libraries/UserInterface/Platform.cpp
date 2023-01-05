#include "SCConfig.h"

#include "Platform.h"
#define SOKOL_APP_IMPL
#define SOKOL_IMPL
#if defined(__APPLE__)
#include <TargetConditionals.h>
#define SOKOL_METAL
#elif defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__linux__)
#define SOKOL_GLCORE33
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES2
#define SOKOL_NO_ENTRY
#endif
// clang-format off
#include "../../Dependencies/sokol/_sokol/sokol_app.h"
#include "../../Dependencies/sokol/_sokol/sokol_gfx.h"
#include "../../Dependencies/sokol/_sokol/sokol_glue.h"
#include "../../Dependencies/sokol/_sokol/sokol_time.h"
#include "../../Dependencies/imgui/_imgui/imgui.h"
#define SOKOL_IMGUI_IMPL
#include "../../Dependencies/sokol/_sokol/util/sokol_imgui.h"
// clang-format on

static sg_pass_action pass_action;
static uint64_t       lastResetTime  = 0;
static bool           shouldBePaused = false;

sg_color gBackgroundValue = {0.0f, 0.5f, 0.7f, 1.0f};

void init(void)
{
#if defined(_WIN32)
    HWND  hwnd  = (HWND)sapp_win32_get_hwnd();
    HICON hIcon = LoadIcon(GetModuleHandleW(NULL), MAKEINTRESOURCE(100));
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
#endif
    // setup sokol-gfx, sokol-time and sokol-imgui
    sg_desc desc = {};
    desc.context = sapp_sgcontext();
    sg_setup(&desc);

    // use sokol-imgui with all default-options (we're not doing
    // multi-sampled rendering or using non-default pixel formats)
    simgui_desc_t simgui_desc = {};
    simgui_setup(&simgui_desc);

    // initial clear color
    pass_action.colors[0].action = SG_ACTION_CLEAR;
    pass_action.colors[0].value  = gBackgroundValue;
    lastResetTime                = stm_now();
}

void frame(void)
{
    const int width             = sapp_width();
    const int height            = sapp_height();
    pass_action.colors[0].value = gBackgroundValue;
    simgui_new_frame({width, height, sapp_frame_duration(), sapp_dpi_scale()});
    platform_draw();
    sg_begin_default_pass(&pass_action, width, height);
    simgui_render();
    sg_end_pass();
    sg_commit();
    if (stm_sec(stm_since(lastResetTime)) > 0.5)
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
#else
        shouldBePaused = true;
#endif
        lastResetTime = stm_now();
    }
}

void cleanup(void)
{
    simgui_shutdown();
    sg_shutdown();
}
#if defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)
_SOKOL_PRIVATE EM_BOOL sapp_emsc_custom_frame(double time, void* userData);
#endif
void input(const sapp_event* ev)
{
    simgui_handle_event(ev);
#if defined(SOKOL_METAL)
#if TARGET_OS_OSX
    NSWindow* window  = (__bridge NSWindow*)sapp_macos_get_window();
    MTKView*  mtkView = window.contentView;
    mtkView.paused    = false;
#else
    UIWindow* window = (__bridge UIWindow*)sapp_ios_get_window();
    MTKView* mtkView = (MTKView*)window.rootViewController.view;
    mtkView.paused = false;
#endif
#elif defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)
    if (shouldBePaused)
    {
        shouldBePaused = false;
        emscripten_request_animation_frame_loop(sapp_emsc_custom_frame, 0);
    }
#endif
    // sapp_set_frame_cb_paused(false);
    lastResetTime = stm_now();
}

sapp_desc sokol_get_desc(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    stm_setup();
    sapp_desc desc                   = {};
    desc.init_cb                     = init;
    desc.frame_cb                    = frame;
    desc.cleanup_cb                  = cleanup;
    desc.event_cb                    = input;
    desc.gl_force_gles2              = true;
    desc.window_title                = "SC Platform Example";
    desc.ios_keyboard_resizes_canvas = false;
    desc.high_dpi                    = true;
    // desc.icon.sokol_default = true;
    desc.enable_clipboard = true;
    return desc;
}

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
    if (shouldBePaused)
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
