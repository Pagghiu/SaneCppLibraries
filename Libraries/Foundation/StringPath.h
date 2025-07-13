// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/StringSpan.h"

namespace SC
{

namespace detail
{
/// @brief A fixed size native string that converts to a StringSpan
template <int N>
struct StringNativeFixed
{
    size_t        length = 0;
    native_char_t buffer[N];

    [[nodiscard]] operator StringSpan() const { return StringSpan({buffer, length}, true, StringEncoding::Native); }
    [[nodiscard]] StringSpan          view() const { return *this; }
    [[nodiscard]] Span<native_char_t> writableSpan() const { return {buffer, N}; }

    [[nodiscard]] bool assign(StringSpan str)
    {
        length = 0;
        return append(str);
    }

    [[nodiscard]] bool append(StringSpan str)
    {
        StringSpan::NativeWritable string = {{buffer, N}, length};
        if (not str.appendNullTerminatedTo(string))
            return false;
        length = string.length;
        return true;
    }
};
} // namespace detail

/// @brief Pre-sized char array holding enough space to represent a file system path
struct StringPath
{
    /// @brief Maximum size of paths on current native platform
#if SC_PLATFORM_WINDOWS
    static constexpr size_t MaxPath = 260; // Equal to 'MAX_PATH' on Windows
#elif SC_PLATFORM_APPLE
    static constexpr size_t MaxPath = 1024; // Equal to 'PATH_MAX' on macOS
#else
    static constexpr size_t MaxPath = 4096; // Equal to 'PATH_MAX' on Linux
#endif
    size_t length = 0; ///< Length of the path in bytes (excluding null terminator)
#if SC_PLATFORM_WINDOWS
    wchar_t path[MaxPath]; ///< Native path on Windows (UTF16-LE)
    operator StringSpan() const { return StringSpan({path, length}, true); }
#else
    char path[MaxPath]; ///< Native path on Posix (UTF-8)
    operator StringSpan() const { return StringSpan({path, length}, true, StringEncoding::Utf8); }
#endif

    /// @brief Obtain a StringSpan from the current StringPath
    [[nodiscard]] StringSpan view() const { return *this; }

    /// @brief Assigns a StringView to current StringPath, converting the encoding from UTF16 to UTF8 if needed
    [[nodiscard]] bool assign(StringSpan str) { return str.writeNullTerminatedTo({path, MaxPath}, length); }
};
} // namespace SC
