// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_PRIMITIVE_TYPES_DEFINITION_H
#if SC_FOUNDATION_PRIMITIVE_TYPES_DEFINITION_H != 1
#error "PrimitiveTypesDefinition.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PRIMITIVE_TYPES_DEFINITION_H 1 // Increment to indicate a new version of the file

namespace SC
{
// clang-format off
//! @addtogroup group_foundation_utility
//! @{
#if defined(_WIN32) || defined(_WIN64)
using native_char_t = wchar_t;

using uint8_t  = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;

using int8_t  = signed char;
using int16_t = short;
using int32_t = int;
using int64_t = long long;
#else
using native_char_t = char; ///< The native char for the platform (wchar_t (4 bytes) on Windows, char (1 byte) everywhere else )

using uint8_t  = unsigned char;     ///< Platform independent (1) byte unsigned int
using uint16_t = unsigned short;    ///< Platform independent (2) bytes unsigned int
using uint32_t = unsigned int;      ///< Platform independent (4) bytes unsigned int
#if defined(__linux__)
using uint64_t = unsigned long int; ///< Platform independent (8) bytes unsigned int
#else
using uint64_t = unsigned long long;///< Platform independent (8) bytes unsigned int
#endif
using int8_t  = signed char;        ///< Platform independent (1) byte signed int
using int16_t = short;              ///< Platform independent (2) bytes signed int
using int32_t = int;                ///< Platform independent (4) bytes signed int
#if defined(__linux__)
using int64_t = signed long int;    ///< Platform independent (8) bytes signed int
#else
using int64_t = long long;          ///< Platform independent (8) bytes signed int
#endif
#endif

using size_t  = decltype(sizeof(0));                                                 ///< Platform independent unsigned size type
using ssize_t = decltype(static_cast<char*>(nullptr) - static_cast<char*>(nullptr)); ///< Platform independent signed size type

/// @brief A vocabulary type representing a time interval in milliseconds since epoch
struct TimeMs
{
    int64_t milliseconds = 0;
};
//! @}
} // namespace SC

#endif // SC_FOUNDATION_PRIMITIVE_TYPES_DEFINITION_H
