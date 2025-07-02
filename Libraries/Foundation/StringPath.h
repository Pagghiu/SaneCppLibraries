// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/StringViewData.h"

namespace SC
{
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
    operator StringViewData() const { return StringViewData({path, length}, true); }
#else
    char path[MaxPath]; ///< Native path on Posix (UTF-8)
    operator StringViewData() const { return StringViewData({path, length}, true, StringEncoding::Utf8); }
#endif

    /// @brief Obtain a StringViewData from the current StringPath
    [[nodiscard]] StringViewData view() const { return *this; }

    /// @brief Assigns a StringView to current StringPath, converting the encoding from UTF16 to UTF8 if needed
    [[nodiscard]] bool assign(StringViewData pathToConvert);
};
} // namespace SC
