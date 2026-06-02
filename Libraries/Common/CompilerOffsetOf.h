// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_COMPILER_OFFSET_OF_DEFINITION_H
#if SC_FOUNDATION_COMPILER_OFFSET_OF_DEFINITION_H != 1
#error "CompilerOffsetOf.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_OFFSET_OF_DEFINITION_H 1 // Increment to indicate a new version of the file

/// Returns offset of Class::Field in bytes
#define SC_COMPILER_OFFSETOF(Class, Field)            __builtin_offsetof(Class, Field)

namespace SC
{
// clang-format off
template <int offset, typename T, typename R> T& fieldOffset(R& object) { return *reinterpret_cast<T*>(reinterpret_cast<char*>(&object) - offset); }
#define SC_COMPILER_FIELD_OFFSET(Class, Field, Value)  SC::fieldOffset<SC_COMPILER_OFFSETOF(Class, Field), Class, decltype(Class::Field)>(Value);
// clang-format on
} // namespace SC

/// Disables invalid-offsetof gcc and clang warning
#if defined(__clang__)
#define SC_COMPILER_WARNING_PUSH_OFFSETOF                                                                              \
    _Pragma("clang diagnostic push");                                                                                  \
    _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"");
#define SC_COMPILER_WARNING_POP_OFFSETOF _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define SC_COMPILER_WARNING_PUSH_OFFSETOF                                                                              \
    _Pragma("GCC diagnostic push");                                                                                    \
    _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"");
#define SC_COMPILER_WARNING_POP_OFFSETOF _Pragma("GCC diagnostic pop")
#else
#define SC_COMPILER_WARNING_PUSH_OFFSETOF _Pragma("warning(push)")
#define SC_COMPILER_WARNING_POP_OFFSETOF  _Pragma("warning(pop)")
#endif
#endif // SC_FOUNDATION_COMPILER_OFFSET_OF_DEFINITION_H
