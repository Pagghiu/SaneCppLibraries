// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Internal/IGrowableBuffer.h"
#include "../Foundation/StringSpan.h"

namespace SC
{
//! @addtogroup group_strings
//! @{

/// @brief Converts String to a different encoding (UTF8, UTF16).
///
/// SC::StringConverter converts strings between different UTF encodings and can add null-terminator if requested.
/// When the SC::StringSpan is already null-terminated, the class just forwards the original SC::StringSpan.
///
/// Example:
/// \snippet Tests/Libraries/Strings/StringConverterTest.cpp stringConverterTestSnippet
struct SC_COMPILER_EXPORT StringConverter
{
    /// @brief Specifies if to add a null terminator
    enum StringTermination
    {
        NullTerminate, ///< A null terminator will be added at the end of the String
        DoNotTerminate ///< A null terminator will NOT be added at the end of the String
    };

    /// @brief Appends to buffer text with requested encoding, optionally null-terminating it too.
    /// @param encoding The requested destination encoding to convert to
    /// @param text The StringSpan to be converted
    /// @param buffer Encoded text will be appended to buffer
    /// @param nullTerminate Specifies if the StringSpan will need to be null terminated or not
    /// @return `true` if the conversion succeeds
    template <typename T>
    [[nodiscard]] static bool appendEncodingTo(StringEncoding encoding, StringSpan text, T& buffer,
                                               StringTermination nullTerminate)
    {
        GrowableBuffer<T> growableBuffer{buffer};
        return appendEncodingTo(encoding, text, static_cast<IGrowableBuffer&>(growableBuffer), nullTerminate);
    }

    [[nodiscard]] static bool appendEncodingTo(StringEncoding encoding, StringSpan text, IGrowableBuffer& buffer,
                                               StringTermination nullTerminate);
    struct Internal;
};
//! @}
} // namespace SC
