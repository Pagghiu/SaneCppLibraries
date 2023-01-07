#include "SCConfig.h"

// clang-format off
#if SC_ENABLE_FREETYPE

#if _WIN32 || _WIN64
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_WARNINGS 1
#endif
#define FT2_BUILD_LIBRARY // otherwise FT_ERR_PREFIX will be undefined

#if _MSC_VER
#pragma warning (push)
#pragma warning (disable:4244 4267)
#elif __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wdeprecated-volatile"                    // Emscripten
#pragma clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"   // Emscripten
#elif __GCC__
#endif
// Point configuration to the config subdirectory
#ifndef FT_CONFIG_CONFIG_H
#define FT_CONFIG_CONFIG_H  <freetype/../../../config/ftconfig.h>
#endif
#ifndef FT_CONFIG_OPTIONS_H
#define FT_CONFIG_OPTIONS_H  <freetype/../../../config/ftoption.h>
#endif

#include "_freetype/src/autofit/autofit.c"
#include "_freetype/src/base/ftbase.c"
#include "_freetype/src/base/ftbbox.c"
#include "_freetype/src/base/ftbdf.c"
#include "_freetype/src/base/ftbitmap.c"
#include "_freetype/src/base/ftcid.c"
#include "_freetype/src/base/ftfstype.c"
#include "_freetype/src/base/ftgasp.c"
#include "_freetype/src/base/ftglyph.c"
#include "_freetype/src/base/ftgxval.c"
#include "_freetype/src/base/ftinit.c"
#include "_freetype/src/base/ftmm.c"
#include "_freetype/src/base/ftotval.c"
#include "_freetype/src/base/ftpatent.c"
#include "_freetype/src/base/ftpfr.c"
#include "_freetype/src/base/ftstroke.c"
#include "_freetype/src/base/ftsynth.c"
#include "_freetype/src/base/fttype1.c"
#include "_freetype/src/base/ftwinfnt.c"
#include "_freetype/src/bdf/bdf.c"
#include "_freetype/src/bzip2/ftbzip2.c"
#include "_freetype/src/cache/ftcache.c"
#include "_freetype/src/cff/cff.c"
#include "_freetype/src/cid/type1cid.c"
#include "_freetype/src/gzip/ftgzip.c"
#include "_freetype/src/lzw/ftlzw.c"
#include "_freetype/src/pcf/pcf.c"
#ifdef local
#undef local
#endif
#include "_freetype/src/pfr/pfr.c"
#include "_freetype/src/psaux/psaux.c"
#include "_freetype/src/pshinter/pshinter.c"
#include "_freetype/src/psnames/psnames.c"
#include "_freetype/src/raster/raster.c"
#include "_freetype/src/sdf/sdf.c"
#include "_freetype/src/sfnt/sfnt.c"
#undef ONE_PIXEL
#include "_freetype/src/smooth/smooth.c"
#include "_freetype/src/svg/svg.c"
#include "_freetype/src/truetype/truetype.c"
#include "_freetype/src/type1/type1.c"
#ifdef N
#undef N
#endif
#include "_freetype/src/type42/type42.c"
#include "_freetype/src/winfonts/winfnt.c"

#if _WIN32 || _WIN64
#include "_freetype/builds/windows/ftsystem.c"
#include "_freetype/builds/windows/ftdebug.c"
#elif __EMSCRIPTEN__ || __linux__ || __APPLE__
#include "_freetype/builds/unix/ftsystem.c"
#include "_freetype/src/base/ftdebug.c"
#else
#include "_freetype/src/base/ftsystem.c"
#endif

#if _MSC_VER
#pragma warning (pop)
#elif __clang__
#pragma clang diagnostic pop
#else

#endif
#endif
// clang-format on
