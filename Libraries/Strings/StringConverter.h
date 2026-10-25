// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/IGrowableBufferSpan.h"
#include "../Common/StringSpan.h"
#include "StringsExport.h"

namespace SC
{
//! @addtogroup group_strings
//! @{

/// @brief Converts String to a different encoding (UTF8, UTF16).
///
/// SC::StringConverter converts strings between different UTF encodings and can add null-terminator if requested.
/// Output is appended to the supplied buffer, including a terminator when requested.
/// On Linux, cross-encoding conversion rejects malformed UTF8/UTF16 before changing the buffer and accepts embedded
/// NUL. It requires only the exact output capacity, including the existing prefix and requested terminator. In that
/// fallback, a source wholly inside the existing logical buffer remains valid across growth; other overlaps are not
/// covered.
///
/// Example:
/// \snippet Tests/Libraries/Strings/StringConverterTest.cpp stringConverterTestSnippet
struct SC_STRINGS_EXPORT StringConverter
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
