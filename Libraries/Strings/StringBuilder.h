// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringFormat.h"
namespace SC
{
//! @addtogroup group_strings
//! @{

/// @brief Builds String out of a sequence of StringView or formatting through StringFormat
///
/// The output can be a SC::Buffer or a SC::SmallBuffer (see [Foundation](@ref library_foundation))
/// One can do a:
/// - One-shot StringBuilder::format
/// - Create a StringBuilder object and do multiple appends + StringBuilder::finalize
///
struct SC_COMPILER_EXPORT StringBuilder
{
    /// @brief Clearing flags used when initializing destination buffer
    enum Flags
    {
        Clear,     ///< Destination buffer will be cleared before pushing to it
        DoNotClear ///< Destination buffer will not be cleared before pushing to it
    };
    /// @brief Create a StringBuilder that will push to given Buffer, with specific encoding.
    /// @param buffer Destination buffer where code points will be pushed
    /// @param encoding The encoding to be used
    /// @param flags Specifies if destination buffer must be emptied or not before pushing
    template <typename T>
    StringBuilder(T& buffer, StringEncoding encoding, Flags flags = DoNotClear)
    {
        GrowableBuffer<T>& bufferT = bufferStorage.reinterpret_as<GrowableBuffer<T>>();
        placementNew(bufferT, buffer);
        initWithEncoding(bufferT, encoding, flags);
    }

    /// @brief Create a StringBuilder that will push to given Buffer, with specific encoding.
    /// @param string Destination string that will be filled according to string encoding
    /// @param flags Specifies if destination string must be emptied or not before pushing
    template <typename T>
    StringBuilder(T& string, Flags flags = DoNotClear)
    {
        GrowableBuffer<T>& bufferT = bufferStorage.reinterpret_as<GrowableBuffer<T>>();
        placementNew(bufferT, string);
        initWithEncoding(bufferT, GrowableBuffer<T>::getEncodingFor(string), flags);
    }

    StringBuilder(IGrowableBuffer& bufferT, StringEncoding encoding, Flags flags);

    ~StringBuilder();

    /// @brief Finalizes building the string and returns a StringView with the contents
    StringView finalize();

    /// @brief Obtains view after finalize has been previously called
    /// @warning Calling this method before finalize() will assert
    [[nodiscard]] StringView view();

    /// @brief Uses StringFormat to format the given StringView against args, replacing destination contents.
    /// @tparam Types Type of Args
    /// @param buffer The destination buffer that will hold the result of format
    /// @param fmt The format strings
    /// @param args arguments to format
    /// @return `true` if format succeeded
    /// @n
    /// @code{.cpp}
    /// String buffer(StringEncoding::Ascii); // Or SmallString<N>
    /// SC_TRY(StringBuilder::format(buffer, "[{1}-{0}]", "Storia", "Bella"));
    /// SC_ASSERT_RELEASE(buffer.view() == "[Bella-Storia]");
    /// @endcode
    template <typename T, typename... Types>
    [[nodiscard]] static bool format(T& buffer, StringView fmt, Types&&... args);

    /// @brief Assigns StringView to destination buffer
    /// @param buffer The destination buffer that will hold the result of format
    /// @param text StringView to assign to destination buffer
    /// @return `true` if assign succeeded
    template <typename T>
    [[nodiscard]] static bool format(T& buffer, StringView text);

    /// @brief Uses StringFormat to format the given StringView against args, appending to destination contents.
    /// @tparam Types Type of Args
    /// @param fmt The format strings
    /// @param args arguments to format
    /// @return `true` if format succeeded
    /// @n
    /// Example:
    /// @code{.cpp}
    /// String        buffer(StringEncoding::Ascii); // Or SmallString<N>
    /// StringBuilder builder(buffer);
    /// SC_TRY(builder.append("Salve"));
    /// SC_TRY(builder.append(" {1} {0}!!!", "tutti", "a"));
    /// SC_ASSERT_RELEASE(builder.finalize() == "Salve a tutti!!!");
    /// @endcode
    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types&&... args);

    /// @brief Appends StringView to destination buffer
    /// @param str StringView to append to destination buffer
    /// @return `true` if append succeeded
    [[nodiscard]] bool append(StringView str);

    /// @brief Appends source to destination buffer, replacing `occurrencesOf` StringView with StringView `with`
    /// @param source The StringView to be appended
    /// @param occurrencesOf The StringView to be searched inside `source`
    /// @param with The replacement StringView to be written in destination buffer
    /// @return `true` if append succeeded
    ///
    /// Example:
    /// @snippet Tests/Libraries/Strings/StringBuilderTest.cpp stringBuilderTestAppendReplaceAllSnippet
    [[nodiscard]] bool appendReplaceAll(StringView source, StringView occurrencesOf, StringView with);

    /// @brief Option for StringBuilder::appendHex
    enum class AppendHexCase
    {
        UpperCase,
        LowerCase,
    };
    /// @brief Appends given binary data escaping it as hexadecimal ASCII characters
    /// @param data Binary data to append to destination buffer
    /// @param casing Specifies if it should be appended using upper case or lower case
    /// @return `true` if append succeeded
    ///
    /// Example:
    /// @snippet Tests/Libraries/Strings/StringBuilderTest.cpp stringBuilderTestAppendHexSnippet
    [[nodiscard]] bool appendHex(Span<const uint8_t> data, AppendHexCase casing);

  private:
    void initWithEncoding(IGrowableBuffer& bufferT, StringEncoding stringEncoding, Flags flags);
    void clear();

    IGrowableBuffer* buffer        = nullptr;
    bool             destroyBuffer = true;

    AlignedStorage<6 * sizeof(void*)> bufferStorage;

    StringEncoding encoding;
    StringView     finalizedView;
};
//! @}

} // namespace SC

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
template <typename T>
bool SC::StringBuilder::format(T& buffer, StringView fmt)
{
    StringBuilder sb(buffer, Clear);
    const bool    res = sb.append(fmt);
    sb.finalize();
    return res;
}

template <typename T, typename... Types>
inline bool SC::StringBuilder::format(T& buffer, StringView fmt, Types&&... args)
{
    StringBuilder sb(buffer, Clear);
    const bool    res = sb.append(fmt, forward<Types>(args)...);
    sb.finalize();
    return res;
}

template <typename... Types>
inline bool SC::StringBuilder::append(StringView fmt, Types&&... args)
{
    SC_TRY(fmt.getEncoding() != StringEncoding::Utf16); // UTF16 format strings are not supported
    // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
    // because on a valid UTF8 string, these chars are unambiguously recognizable
    StringFormatOutput sfo(encoding, *buffer);
    return StringFormat<StringIteratorASCII>::format(sfo, fmt, forward<Types>(args)...);
}
