#include "SCConfig.h"

#include "DependencyImgui.h"
#if SC_ENABLE_FREETYPE
#define IMGUI_ENABLE_FREETYPE
#endif

// clang-format off
#include "_imgui/imgui.cpp"
#include "_imgui/imgui_draw.cpp"
#include "_imgui/imgui_tables.cpp"
#include "_imgui/imgui_widgets.cpp"
#include "_imgui/imgui_demo.cpp"
// clang-format on

#if SC_ENABLE_FREETYPE
#if _MSC_VER
#pragma warning (push)
#elif __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#elif __GCC__
#endif
#ifndef FT_CONFIG_CONFIG_H
#define FT_CONFIG_CONFIG_H  <freetype/../../../config/ftconfig.h>
#endif
#ifndef FT_CONFIG_OPTIONS_H
#define FT_CONFIG_OPTIONS_H  <freetype/../../../config/ftoption.h>
#endif

#include "_imgui/misc/freetype/imgui_freetype.cpp"

#if _MSC_VER
#pragma warning (pop)
#elif __clang__
#pragma clang diagnostic pop
#else
#endif
#endif // IMGUI_ENABLE_FREETYPE

#define SOKOL_IMGUI_IMPL
#include "../sokol/DependencySokol.h"
#include "../sokol/_sokol/util/sokol_imgui.h"
void sokol_delay_init_imgui(void)
{
    ImGuiIO*       io = &ImGui::GetIO();
    unsigned char* font_pixels;
    int            font_width, font_height;
    io->Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc            = {};
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
}
