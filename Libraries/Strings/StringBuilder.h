// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringFormat.h"
namespace SC
{
template <typename T>
struct StringBuilderFor;
//! @addtogroup group_strings
//! @{

/// @brief Builds String out of a sequence of StringView or formatting through StringFormat
///
/// The output can be a SC::Buffer or a SC::SmallBuffer (see [Foundation](@ref library_foundation))
/// One can do a:
/// - One-shot StringBuilder::format
/// - Multiple appends using StringBuilder::create or StringBuilder::createForAppendingTo followed by
/// StringBuilder::finalize
///
/// StringBuilder::format example:
/// @snippet Tests/Libraries/Strings/StringBuilderTest.cpp stringBuilderFormatSnippet
///
/// StringBuilder::create example:
/// @snippet Tests/Libraries/Strings/StringBuilderTest.cpp stringBuilderTestAppendSnippet
struct SC_COMPILER_EXPORT StringBuilder
{
    // clang-format off
    /// @brief Creates a StringBuilder for the given string or buffer, replacing its current contents
    template <typename T> static StringBuilderFor<T> create(T& stringOrBuffer) noexcept { return {stringOrBuffer, StringBuilder::Clear}; }

    /// @brief Creates a StringBuilder for the given string or buffer, appending to its current contents
    template <typename T> static StringBuilderFor<T> createForAppendingTo(T& stringOrBuffer) noexcept { return {stringOrBuffer, StringBuilder::Append}; }

    /// @brief Helper to format a StringView against args, replacing destination contents, in a single function call
    template <typename T, typename... Types>
    [[nodiscard]] static bool format(T& buffer, StringView fmt, Types&&... args) { return StringBuilder::create(buffer).append(fmt, forward<Types>(args)...); }
    // clang-format on

    /// @brief Formats the given StringView against args, appending to destination contents
    template <typename... Types>
    [[nodiscard]] bool append(StringView fmt, Types&&... args)
    {
        // It's ok parsing format string '{' and '}' both for utf8 and ascii with StringIteratorASCII
        // because on a valid UTF8 string, these chars are unambiguously recognizable
        StringFormatOutput sfo(encoding, *buffer);
        return StringFormat<StringIteratorASCII>::format(sfo, fmt, forward<Types>(args)...);
    }

    /// @brief Appends StringView to destination buffer
    /// @param str StringView to append to destination buffer
    [[nodiscard]] bool append(StringView str);

    /// @brief Appends source to destination buffer, replacing `occurrencesOf` StringView with StringView `with`
    /// @param source The StringView to be appended
    /// @param occurrencesOf The StringView to be searched inside `source`
    /// @param with The replacement StringView to be written in destination buffer
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

  protected:
    enum Flags
    {
        Clear, ///< Destination buffer will be cleared before pushing to it
        Append ///< Destination buffer will not be cleared before pushing to it
    };
    friend struct Path;

    StringBuilder() = default;
    StringBuilder(IGrowableBuffer& ibuffer, StringEncoding encoding, Flags flags) noexcept;
    void initWithEncoding(IGrowableBuffer& bufferT, StringEncoding stringEncoding, Flags flags) noexcept;

    IGrowableBuffer* buffer = nullptr;
    StringEncoding   encoding;
};

/// @brief StringBuilder tied to a specific type, created through StringBuilder::create or
/// StringBuilder::createForAppendingTo
template <typename T>
struct StringBuilderFor : public StringBuilder
{
    GrowableBuffer<T> growableBuffer;
    StringView        finalizedView;
    StringBuilderFor(T& stringOrBuffer, Flags flags) noexcept : growableBuffer(stringOrBuffer)
    {
        initWithEncoding(growableBuffer, GrowableBuffer<T>::getEncodingFor(stringOrBuffer), flags);
    }

    ~StringBuilderFor() noexcept { finalize(); }

    /// @brief Finalizes the StringBuilder, returning the resulting StringView
    StringView finalize() noexcept
    {
        if (buffer)
        {
            growableBuffer.finalize();
            finalizedView = {{buffer->data(), buffer->size()}, true, encoding};
            buffer        = nullptr;
        }
        return view();
    }

    /// @brief Returns the resulting StringView after finalize
    /// @warning This method can be called only after finalize has been called
    [[nodiscard]] StringView view() noexcept
    {
        SC_ASSERT_RELEASE(buffer == nullptr);
        return finalizedView;
    }
};

//! @}

} // namespace SC
