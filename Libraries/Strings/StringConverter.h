// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringView.h"

namespace SC
{
template <typename T>
struct Vector;
struct SC_COMPILER_EXPORT StringConverter;
struct String;
} // namespace SC

//! @addtogroup group_strings
//! @{

/// @brief Converts String to a different encoding (UTF8, UTF16).
///
/// SC::StringConverter converts strings between different UTF encodings and can add null-terminator if requested.
/// When the SC::StringView is already null-terminated, the class just forwards the original SC::StringView.
///
/// Example:
/// \snippet Libraries/Strings/Tests/StringConverterTest.cpp stringConverterTestSnippet
struct SC::StringConverter
{
    /// @brief Specifies if to add a null terminator
    enum NullTermination
    {
        AddZeroTerminator,     ///< A null terminator will be added at the end of the String
        DoNotAddZeroTerminator ///< A null terminator will NOT be added at the end of the String
    };

    /// @brief Converts text to (eventually null terminated) UTF8 encoding. Uses the passed in buffer if necessary.
    /// @param text The StringView to be converted
    /// @param buffer The destination buffer that will be eventually used
    /// @param encodedText If specified, a StringView containing the encoded text will be returned
    /// @param nullTerminate Specifies if the StringView will need to be null terminated or not
    /// @return `true` if the conversion succeeds
    [[nodiscard]] static bool convertEncodingToUTF8(StringView text, Vector<char>& buffer,
                                                    StringView*     encodedText   = nullptr,
                                                    NullTermination nullTerminate = AddZeroTerminator);

    /// @brief Converts text to (eventually null terminated) UTF16 encoding. Uses the passed in buffer if necessary.
    /// @param text The StringView to be converted
    /// @param buffer The destination buffer that will be eventually used
    /// @param encodedText If specified, a StringView containing the encoded text will be returned
    /// @param nullTerminate Specifies if the StringView will need to be null terminated or not
    /// @return `true` if the conversion succeeds
    [[nodiscard]] static bool convertEncodingToUTF16(StringView text, Vector<char>& buffer,
                                                     StringView*     encodedText   = nullptr,
                                                     NullTermination nullTerminate = AddZeroTerminator);

    /// @brief Converts text to (eventually null terminated) requested encoding. Uses the passed in buffer if necessary.
    /// @param encoding The requested destination encoding to convert to
    /// @param text The StringView to be converted
    /// @param buffer The destination buffer that will be eventually used
    /// @param encodedText If specified, a StringView containing the encoded text will be returned
    /// @param nullTerminate Specifies if the StringView will need to be null terminated or not
    /// @return `true` if the conversion succeeds
    [[nodiscard]] static bool convertEncodingTo(StringEncoding encoding, StringView text, Vector<char>& buffer,
                                                StringView*     encodedText   = nullptr,
                                                NullTermination nullTerminate = AddZeroTerminator);

    /// @brief Clearing flags used when initializing destination buffer
    enum Flags
    {
        Clear,     ///< Destination buffer will be cleared before pushing to it
        DoNotClear ///< Destination buffer will not be cleared before pushing to it
    };

    /// @brief Create a StringBuilder that will push to given String
    /// @param text Destination buffer where code points will be pushed
    /// @param flags Specifies if destination buffer must be emptied or not before pushing
    StringConverter(String& text, Flags flags = DoNotClear);

    /// @brief Create a StringBuilder that will push to given Vector, with specific encoding.
    /// @param text Destination buffer where code points will be pushed
    /// @param encoding The encoding to be used
    StringConverter(Vector<char>& text, StringEncoding encoding);

    /// @brief Converts a given input StringView to null-terminated version.
    /// Uses supplied buffer in constructor if an actual conversion is needed.
    /// @param input The StringView to be converted
    /// @param encodedText The converted output StringView
    /// @return `true` if the conversion succeeded
    [[nodiscard]] bool convertNullTerminateFastPath(StringView input, StringView& encodedText);

    /// @brief Appends the given StringView and adds null-terminator.
    /// If existing null-terminator was already last inserted code point, it will be removed before appending input.
    /// @param input The StringView to be appended
    /// @param popExistingNullTerminator If true, removes existing null terminator before adding the new one
    /// @return `true` if the StringView has been successfully appended
    [[nodiscard]] bool appendNullTerminated(StringView input, bool popExistingNullTerminator = true);

  private:
    /// @brief Removes ending null-terminator from stringData if stringData is not empty
    /// @param stringData The buffer to be modified
    /// @param encoding The requested encoding, that determines how many null-termination bytes exist
    /// @return `false` if the stringData is empty or its size is insufficient considering the given encoding
    [[nodiscard]] static bool popNullTermIfNotEmpty(Vector<char>& stringData, StringEncoding encoding);

    /// @brief Will unconditionally add a null-terminator to given buffer.
    /// @param stringData The destination buffer
    /// @param encoding The given encoding
    /// @return `true` if null-terminator was successfully pushed
    [[nodiscard]] static bool pushNullTerm(Vector<char>& stringData, StringEncoding encoding);

    /// @brief Eventually add null-terminators if needed to end of given buffer
    /// @param data The destination buffer
    /// @param encoding The given encoding
    /// @return `true` if null-terminator was successfully pushed
    [[nodiscard]] static bool ensureZeroTermination(Vector<char>& data, StringEncoding encoding);

    void internalClear();
    // TODO: FileSystemIterator should just use a Vector<char>
    friend struct FileSystemIterator;
    template <int N>
    friend struct SmallString;
    friend struct StringBuilder;
    friend struct File;

    [[nodiscard]] bool        setTextLengthInBytesIncludingTerminator(size_t newDataSize);
    [[nodiscard]] static bool convertSameEncoding(StringView text, Vector<char>& buffer, StringView* encodedText,
                                                  NullTermination terminate);
    static void               eventuallyNullTerminate(Vector<char>& buffer, StringEncoding destinationEncoding,
                                                      StringView* encodedText, NullTermination terminate);

    StringEncoding encoding;
    Vector<char>&  data;
    // Appends the input string null terminated
    [[nodiscard]] bool internalAppend(StringView input, StringView* encodedText);

    // Fallbacks for platforms without an API to do the conversion out of the box (Linux)
    [[nodiscard]] static bool convertUTF16LE_to_UTF8(const StringView sourceUtf16, Vector<char>& destination,
                                                     int& writtenCodeUnits);
    [[nodiscard]] static bool convertUTF8_to_UTF16LE(const StringView sourceUtf8, Vector<char>& destination,
                                                     int& writtenCodeUnits);
};
//! @}
