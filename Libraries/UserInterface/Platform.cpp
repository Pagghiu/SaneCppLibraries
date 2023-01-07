#include "SCConfig.h"

#if defined(_WIN32)
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#endif

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
#include "../../Dependencies/sokol/_sokol/sokol_fetch.h"
#include "../../Dependencies/imgui/_imgui/imgui.h"
#if SC_ENABLE_FREETYPE
#include "../../Dependencies/imgui/_imgui/misc/freetype/imgui_freetype.h"
#endif
#define SOKOL_IMGUI_IMPL
#include "../../Dependencies/sokol/_sokol/util/sokol_imgui.h"
// clang-format on

static sg_pass_action gGlobalPassAction;
static uint64_t       gLastResetTime       = 0;
static bool           gShouldBePaused      = false;
static bool           gImguiInited         = false;
static bool           gInitError           = false;
static char           gInitErrorMsg[1024]  = "Error: Cannot load resources";
static const float    gDpiScaling          = 2.0f;
static bool           gFontLoaded[2]       = {false, false};
static int            numResourcesFinished = 0;
static int            numTotalResources    = 0;

sg_color gBackgroundValue = {0.0f, 0.5f, 0.7f, 1.0f};

template <int bufferLength>
const char* GetResourceFile(char (&buffer)[bufferLength], const char* directory, const char* file)
{
    char executablePath[2048] = {0};
    if (executablePath[0] == 0)
    {
#if defined(_WIN32)
        // TODO: This will fail on non ASCII paths
        GetModuleFileNameA(NULL, executablePath, 2048);
        PathRemoveFileSpecA(executablePath);
#elif defined(__APPLE__)
        const char* resourcePath = [NSBundle.mainBundle.resourcePath cStringUsingEncoding:kCFStringEncodingUTF8];
        strncpy(executablePath, resourcePath, 2048);
#elif defined(__EMSCRIPTEN__)
        snprintf(buffer, bufferLength, "%s/%s", directory, file);
#else
#error "TODO: Implement GetResourceFile"
#endif
    }
#if defined(_WIN32)
    snprintf(buffer, bufferLength, "%s\\Resources\\%s\\%s", executablePath, directory, file);
#elif defined(__EMSCRIPTEN__)
    snprintf(buffer, bufferLength, "%s/%s", directory, file);
#elif defined(__APPLE__)
    snprintf(buffer, bufferLength, "%s/%s/%s", executablePath, directory, file);
#else
#error "TODO: Implement GetResourceFile"
#endif
    return buffer;
}

void init_imgui(void)
{
    ImGuiIO* io = &ImGui::GetIO();
    if (gInitError)
    {
        io->Fonts->AddFontDefault();
    }

    unsigned char* font_pixels;
    int            font_width, font_height;
    io->Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc;
    _simgui_clear(&img_desc, sizeof(img_desc));
    img_desc.width                    = font_width;
    img_desc.height                   = font_height;
    img_desc.pixel_format             = SG_PIXELFORMAT_RGBA8;
    img_desc.wrap_u                   = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v                   = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.min_filter               = SG_FILTER_LINEAR;
    img_desc.mag_filter               = SG_FILTER_LINEAR;
    img_desc.data.subimage[0][0].ptr  = font_pixels;
    img_desc.data.subimage[0][0].size = (size_t)(font_width * font_height) * sizeof(uint32_t);
    img_desc.label                    = "sokol-imgui-font";
    _simgui.img                       = sg_make_image(&img_desc);
    io->Fonts->TexID                  = (ImTextureID)(uintptr_t)_simgui.img.id;
    gImguiInited                      = true;
}

static void PlatformFontLoaded(const sfetch_response_t* response)
{
    if (response->fetched)
    {
        ImGuiIO* io = &ImGui::GetIO();
        // Data is owned and will be freed after build
        const uint32_t fontIndex = *((uint32_t*)response->user_data);
        if (fontIndex == 0)
        {
            ImFont* f0 = io->Fonts->AddFontFromMemoryTTF((void*)response->data.ptr, (int)response->data.size,
                                                         16.0f * gDpiScaling);
            f0->Scale /= gDpiScaling;
        }
        else if (fontIndex == 1)
        {
            // static ImWchar ranges[] = { 0x1, 0x1FFFF, 0 };
            static ImWchar      ranges[] = {0x1F354, 0x1F354 + 5, 0};
            static ImFontConfig cfg;
            cfg.OversampleH = cfg.OversampleV = 1;
            // cfg.MergeMode = true;
            cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor | ImGuiFreeTypeBuilderFlags_Bitmap;
            ImFont* f0 = io->Fonts->AddFontFromMemoryTTF((void*)response->data.ptr, (int)response->data.size,
                                                         50 * gDpiScaling, &cfg, ranges);
            f0->Scale /= gDpiScaling;
        }
        gFontLoaded[*((uint32_t*)response->user_data)] = true;
    }

    if (response->finished)
    {
        numResourcesFinished++;
        if (numResourcesFinished == numTotalResources)
        {
            gInitError = false;
            for (int i = 0; i < numTotalResources; ++i)
            {
                if (!gFontLoaded[i])
                {
                    snprintf(gInitErrorMsg, sizeof(gInitErrorMsg), "Error: Cannot load all font resources");
                    gInitError = true;
                }
            }
            init_imgui();
        }
    }
}

void init(void)
{
    // FOR SVG SUPPORT CHECKOUT https://codeberg.org/dnkl/fcft/src/branch/master
    // COLR/CPAL https://github.com/mozilla/twemoji-colr
    // GetResourceFile(buffer, "Fonts", "Noto-COLRv1-emojicompat.ttf");
    // GetResourceFile(buffer, "Fonts", "OpenMoji-Color.ttf");
    // GetResourceFile(buffer, "Fonts", "Twemoji.Mozilla.ttf");
    // GetResourceFile(buffer, "Fonts", "seguiemj.ttf");

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
    simgui_desc_t simgui_desc   = {};
    simgui_desc.no_default_font = true;
    simgui_setup(&simgui_desc);

    sfetch_desc_t fetchSetup = {};
    fetchSetup.num_channels  = 1;
    fetchSetup.num_lanes     = 4;
    sfetch_setup(&fetchSetup);
    uint32_t         first = 0, second = 1;
    char             buffer[2048] = {0};
    sfetch_request_t req          = {};

    req.path           = GetResourceFile(buffer, "Fonts", "DroidSans.ttf");
    req.callback       = PlatformFontLoaded;
    req.user_data.ptr  = &first;
    req.user_data.size = sizeof(first);
    req.buffer.size    = 190044;
    req.buffer.ptr     = malloc(req.buffer.size);
    sfetch_send(&req);
    numTotalResources++;

#if SC_ENABLE_FREETYPE
    req.path           = GetResourceFile(buffer, "Fonts", "NotoColorEmoji-Regular.ttf");
    req.callback       = PlatformFontLoaded;
    req.user_data.ptr  = &second;
    req.user_data.size = sizeof(second);
    req.buffer.size    = 23746536;
    req.buffer.ptr     = malloc(req.buffer.size);
    sfetch_send(&req);
    numTotalResources++;
#endif
    // initial clear color
    gGlobalPassAction.colors[0].action = SG_ACTION_CLEAR;
    gGlobalPassAction.colors[0].value  = gBackgroundValue;
    gLastResetTime                     = stm_now();
}

void frame(void)
{
    sfetch_dowork();
    const int width                   = sapp_width();
    const int height                  = sapp_height();
    gGlobalPassAction.colors[0].value = gBackgroundValue;
    if (gImguiInited)
    {
        simgui_new_frame({width, height, sapp_frame_duration(), sapp_dpi_scale()});
        if (gInitError)
        {
            ImGui::Text("%s", gInitErrorMsg);
        }
        else
        {
            platform_draw();
        }
    }
    sg_begin_default_pass(&gGlobalPassAction, width, height);
    if (gImguiInited)
    {
        simgui_render();
    }
    sg_end_pass();
    sg_commit();
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
        MSG msg;
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
        gShouldBePaused = true;
#endif
        gLastResetTime = stm_now();
    }
}

void cleanup(void)
{
    sfetch_shutdown();
    if (gImguiInited)
    {
        simgui_shutdown();
    }
    sg_shutdown();
}
#if defined(__EMSCRIPTEN__) && defined(SOKOL_NO_ENTRY)
_SOKOL_PRIVATE EM_BOOL sapp_emsc_custom_frame(double time, void* userData);
#endif
void input(const sapp_event* ev)
{
    if (gImguiInited)
    {
        simgui_handle_event(ev);
    }
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
    if (gShouldBePaused)
    {
        gShouldBePaused = false;
        emscripten_request_animation_frame_loop(sapp_emsc_custom_frame, 0);
    }
#endif
    // sapp_set_frame_cb_paused(false);
    gLastResetTime = stm_now();
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
    if (gShouldBePaused)
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
