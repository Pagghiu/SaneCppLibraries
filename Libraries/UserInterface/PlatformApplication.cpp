// clang-format off
#include "SCConfig.h"
// clang-format on

#include "PlatformApplication.h"
#include "PlatformResource.h"

#include "../../Dependencies/imgui/DependencyImgui.h"
#include "../../Dependencies/sokol/DependencySokol.h"
#include "../../Dependencies/stb/DependencyStb.h"

sg_color gBackgroundValue = {0.0f, 0.5f, 0.7f, 1.0f};

static sg_pass_action gGlobalPassAction;

static bool gImguiInited         = false;
static bool gInitError           = false;
static char gInitErrorMsg[1024]  = "Error: Cannot load resources";
static bool gFontLoaded[2]       = {false, false};
static int  numResourcesFinished = 0;
static int  numTotalResources    = 0;

static sg_image gLoadedImage;
static void*    gPixels = nullptr;
static int      png_width, png_height, num_channels;

struct SC::PlatformApplication::Internal
{
    static void onImageFetched(const sfetch_response_t* response)
    {
        if (response->fetched)
        {
            const int desired_channels = 4;
            stbi_uc*  pixels = stbi_load_from_memory((const unsigned char*)response->data.ptr, (int)response->data.size,
                                                     &png_width, &png_height, &num_channels, desired_channels);
            gLoadedImage     = sg_alloc_image();
            if (pixels)
            {
                sg_image_desc desc       = {};
                desc.width               = png_width;
                desc.height              = png_height;
                desc.pixel_format        = SG_PIXELFORMAT_RGBA8;
                desc.wrap_u              = SG_WRAP_CLAMP_TO_EDGE;
                desc.wrap_v              = SG_WRAP_CLAMP_TO_EDGE;
                desc.min_filter          = SG_FILTER_LINEAR;
                desc.mag_filter          = SG_FILTER_LINEAR;
                sg_range subimage        = {};
                subimage.ptr             = pixels;
                subimage.size            = (size_t)(png_width * png_height * 4);
                desc.data.subimage[0][0] = subimage;
                sg_init_image(gLoadedImage, &desc);
                gPixels = pixels;
                // stbi_image_free(pixels);
            }
        }
        if (response->finished)
        {
            numResourcesFinished++;
            if (numResourcesFinished == numTotalResources)
            {
                delayInitImgui();
            }
        }
    }

    static void onFontFeched(const sfetch_response_t* response)
    {
        const float dpiScaling = sapp_dpi_scale();
        if (response->fetched)
        {
            ImGuiIO* io = &ImGui::GetIO();
            // Data is owned and will be freed after build
            const uint32_t fontIndex = *((uint32_t*)response->user_data);
            if (fontIndex == 0)
            {
                ImFont* f0 = io->Fonts->AddFontFromMemoryTTF((void*)response->data.ptr, (int)response->data.size,
                                                             16.0f * dpiScaling);
                f0->Scale /= dpiScaling;
            }
            else if (fontIndex == 1)
            {
                // static ImWchar ranges[] = { 0x1, 0x1FFFF, 0 };
                static const ImWchar ranges[] = {0x1F354, 0x1F354 + 5, 0};
                ImFontConfig         cfg;
                cfg.OversampleH = cfg.OversampleV = 1;
                // cfg.MergeMode = true;
                cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor | ImGuiFreeTypeBuilderFlags_Bitmap;
                ImFont* f0 = io->Fonts->AddFontFromMemoryTTF((void*)response->data.ptr, (int)response->data.size,
                                                             50 * dpiScaling, &cfg, ranges);
                f0->Scale /= dpiScaling;
            }
            gFontLoaded[*((uint32_t*)response->user_data)] = true;
        }

        if (response->finished)
        {
            numResourcesFinished++;
            if (numResourcesFinished == numTotalResources)
            {
                delayInitImgui();
            }
        }
    }

    static void delayInitImgui()
    {
        gInitError = false;
        for (int i = 0; i < sizeof(gFontLoaded); ++i)
        {
            if (!gFontLoaded[i])
            {
                snprintf(gInitErrorMsg, sizeof(gInitErrorMsg), "Error: Cannot load all font resources");
                gInitError = true;
            }
        }
        ImGuiIO* io = &ImGui::GetIO();
        if (gInitError)
        {
            io->Fonts->AddFontDefault();
        }

        sokol_delay_init_imgui();
        gImguiInited = true;
    }

    static void init(void)
    {
        SC::PlatformApplication::initNative();

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

        req.path           = SC::PlatformResourceLoader::lookupPath(buffer, "Fonts", "DroidSans.ttf");
        req.callback       = onFontFeched;
        req.user_data.ptr  = &first;
        req.user_data.size = sizeof(first);
        req.buffer.size    = 190044;
        req.buffer.ptr     = malloc(req.buffer.size);
        sfetch_send(&req);
        numTotalResources++;

#if SC_ENABLE_FREETYPE
        req.path           = SC::PlatformResourceLoader::lookupPath(buffer, "Fonts", "NotoColorEmoji-Regular.ttf");
        req.callback       = onFontFeched;
        req.user_data.ptr  = &second;
        req.user_data.size = sizeof(second);
        req.buffer.size    = 23746536;
        req.buffer.ptr     = malloc(req.buffer.size);
        sfetch_send(&req);
        numTotalResources++;
#endif

        req.path           = SC::PlatformResourceLoader::lookupPath(buffer, "Images", "screenshot-2.png");
        req.callback       = onImageFetched;
        req.user_data.ptr  = &second;
        req.user_data.size = sizeof(second);
        req.buffer.size    = 158111;
        req.buffer.ptr     = malloc(req.buffer.size);
        sfetch_send(&req);
        numTotalResources++;

        // initial clear color
        gGlobalPassAction.colors[0].action = SG_ACTION_CLEAR;
        gGlobalPassAction.colors[0].value  = gBackgroundValue;
    }

    static void frame()
    {
        sfetch_dowork();
        const int width  = sapp_width();
        const int height = sapp_height();
        if (gImguiInited)
        {
            simgui_new_frame({width, height, sapp_frame_duration(), sapp_dpi_scale()});
            if (gInitError)
            {
                ImGui::Text("%s", gInitErrorMsg);
            }
            else
            {
                draw();
                ImTextureID img = (ImTextureID)(uintptr_t)gLoadedImage.id;
                if (ImGui::Button("Save"))
                {
                    saveFiles();
                }
                if (ImGui::Button("Upload"))
                {
                    openFiles();
                }
                ImGui::Image(img, ImVec2(static_cast<float>(png_width) / 2.0f, static_cast<float>(png_height) / 2.0f));
            }
        }
        gGlobalPassAction.colors[0].value = gBackgroundValue;
        sg_begin_default_pass(&gGlobalPassAction, width, height);
        if (gImguiInited)
        {
            simgui_render();
        }
        sg_end_pass();
        sg_commit();
        sokol_pause_rendering();
    }

    static void cleanup()
    {
        sfetch_shutdown();
        if (gImguiInited)
        {
            simgui_shutdown();
        }
        sg_shutdown();
    }

    static void input(const sapp_event* ev)
    {
        if (gImguiInited)
        {
            simgui_handle_event(ev);
        }
        sokol_unpause_rendering();
    }
};

sapp_desc sokol_get_desc(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    stm_setup();
    sapp_desc desc                   = {};
    desc.init_cb                     = SC::PlatformApplication::Internal::init;
    desc.frame_cb                    = SC::PlatformApplication::Internal::frame;
    desc.cleanup_cb                  = SC::PlatformApplication::Internal::cleanup;
    desc.event_cb                    = SC::PlatformApplication::Internal::input;
    desc.gl_force_gles2              = true;
    desc.window_title                = "SC Platform Example";
    desc.ios_keyboard_resizes_canvas = false;
    desc.high_dpi                    = true;
    // desc.icon.sokol_default = true;
    desc.enable_clipboard = true;
    return desc;
}
