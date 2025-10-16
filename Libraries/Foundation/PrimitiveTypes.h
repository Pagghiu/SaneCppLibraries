// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Compiler.h"
#include "../Foundation/Platform.h"

namespace SC
{
// clang-format off
//! @addtogroup group_foundation_utility
//! @{
#if SC_PLATFORM_WINDOWS
using native_char_t = wchar_t;

using uint8_t  = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;

using int8_t  = signed char;
using int16_t = short;
using int32_t = int;
using int64_t = long long;

#if SC_PLATFORM_64_BIT
using size_t  = unsigned __int64;
using ssize_t = signed __int64;
#else

using size_t  = unsigned int;
using ssize_t = long;
#endif
#else
using native_char_t = char; ///< The native char for the platform (wchar_t (4 bytes) on Windows, char (1 byte) everywhere else )

using uint8_t  = unsigned char;     ///< Platform independent (1) byte unsigned int
using uint16_t = unsigned short;    ///< Platform independent (2) bytes unsigned int
using uint32_t = unsigned int;      ///< Platform independent (4) bytes unsigned int
#if SC_PLATFORM_LINUX
using uint64_t = unsigned long int;///< Platform independent (8) bytes unsigned int
#else
using uint64_t = unsigned long long;///< Platform independent (8) bytes unsigned int
#endif
using int8_t  = signed char;        ///< Platform independent (1) byte signed int
using int16_t = short;              ///< Platform independent (2) bytes signed int
using int32_t = int;                ///< Platform independent (4) bytes signed int
#if SC_PLATFORM_LINUX
using int64_t = signed long int;          ///< Platform independent (8) bytes signed int
#else
using int64_t = long long;          ///< Platform independent (8) bytes signed int
#endif
#if SC_PLATFORM_EMSCRIPTEN
using size_t  = unsigned __PTRDIFF_TYPE__;
using ssize_t = signed  __PTRDIFF_TYPE__;
#else
using size_t  = unsigned long;      ///< Platform independent unsigned size type
using ssize_t = signed long;        ///< Platform independent signed size type
#endif
#endif

/// @brief A vocabulary type representing a time interval in milliseconds since epoch
struct TimeMs
{
    int64_t milliseconds = 0;
};
//! @}
} // namespace SC

// clang-format off
namespace SC
{
//! @addtogroup group_foundation_utility
//! @{
/// Tag structure for custom placement new
struct PlacementNew {};
//! @}
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// Custom placement new using SC::PlacementNew class
#if SC_COMPILER_MSVC
inline void* operator new(size_t, void* p, SC::PlacementNew) noexcept { return p; }
inline void* operator new[](size_t, void* p, SC::PlacementNew) noexcept { return p; }
inline void  operator delete(void*, void*, SC::PlacementNew) noexcept {}
#else
inline void* operator new(SC::size_t, void* p, SC::PlacementNew) noexcept { return p; }
inline void* operator new[](SC::size_t, void* p, SC::PlacementNew) noexcept { return p; }
#endif
namespace SC
{
/// Placement New
template<typename T, typename... Q> void placementNew(T& storage, Q&&... other) { new (&storage, PlacementNew()) T(SC::forward<Q>(other)...); }
template<typename T> void placementNewArray(T* storage, size_t size) { new (storage, PlacementNew()) T[size]; }
template<typename T> void dtor(T& t){ t.~T(); }
}
//! @}
// clang-format on
