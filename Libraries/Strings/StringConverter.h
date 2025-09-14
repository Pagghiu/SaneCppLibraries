// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringView.h"

namespace SC
{
struct Buffer;

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
    enum NullTermination
    {
        AddZeroTerminator,     ///< A null terminator will be added at the end of the String
        DoNotAddZeroTerminator ///< A null terminator will NOT be added at the end of the String
    };

    /// @brief Converts text to (eventually null terminated) UTF8 encoding. Uses the passed in buffer if necessary.
    /// @param text The StringSpan to be converted
    /// @param buffer The destination buffer that will be eventually used
    /// @param encodedText If specified, a StringSpan containing the encoded text will be returned
    /// @param nullTerminate Specifies if the StringSpan will need to be null terminated or not
    /// @return `true` if the conversion succeeds
    [[nodiscard]] static bool convertEncodingToUTF8(StringSpan text, Buffer& buffer, StringSpan* encodedText = nullptr,
                                                    NullTermination nullTerminate = AddZeroTerminator);

    /// @brief Converts text to (eventually null terminated) UTF16 encoding. Uses the passed in buffer if necessary.
    /// @param text The StringSpan to be converted
    /// @param buffer The destination buffer that will be eventually used
    /// @param encodedText If specified, a StringSpan containing the encoded text will be returned
    /// @param nullTerminate Specifies if the StringSpan will need to be null terminated or not
    /// @return `true` if the conversion succeeds
    [[nodiscard]] static bool convertEncodingToUTF16(StringSpan text, Buffer& buffer, StringSpan* encodedText = nullptr,
                                                     NullTermination nullTerminate = AddZeroTerminator);

    /// @brief Converts text to (eventually null terminated) requested encoding. Uses the passed in buffer if necessary.
    /// @param encoding The requested destination encoding to convert to
    /// @param text The StringSpan to be converted
    /// @param buffer The destination buffer that will be eventually used
    /// @param encodedText If specified, a StringSpan containing the encoded text will be returned
    /// @param nullTerminate Specifies if the StringSpan will need to be null terminated or not
    /// @return `true` if the conversion succeeds
    [[nodiscard]] static bool convertEncodingTo(StringEncoding encoding, StringSpan text, Buffer& buffer,
                                                StringSpan*     encodedText   = nullptr,
                                                NullTermination nullTerminate = AddZeroTerminator);

  private:
    static void ensureZeroTermination(Buffer& data, StringEncoding encoding);

    [[nodiscard]] static bool popNullTermIfNotEmpty(Buffer& stringData, StringEncoding encoding);
    [[nodiscard]] static bool pushNullTerm(Buffer& stringData, StringEncoding encoding);

    friend struct String;
    friend struct StringBuilder;
    struct Internal;
};
//! @}
} // namespace SC
