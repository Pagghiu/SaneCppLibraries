// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Compiler.h"
#include "Platform.h"
namespace SC
{
#if SC_PLATFORM_WINDOWS
using utf_char_t = wchar_t;
using char_t     = char;

using uint8_t  = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;

using int8_t  = signed char;
using int16_t = short;
using int32_t = int;
using int64_t = long long;

using size_t  = unsigned __int64;
using ssize_t = signed __int64;
#else
using utf_char_t = char;
using char_t     = char;

using uint8_t  = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;

using int8_t  = signed char;
using int16_t = short;
using int32_t = int;
using int64_t = long long;
#if SC_PLATFORM_EMSCRIPTEN
using size_t  = unsigned __PTRDIFF_TYPE__;
using ssize_t = signed  __PTRDIFF_TYPE__;
#else
using size_t  = unsigned long;
using ssize_t = signed long;
#endif
#endif
} // namespace SC
