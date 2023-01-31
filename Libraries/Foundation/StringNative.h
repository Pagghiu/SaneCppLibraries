// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SmallVector.h"
#include "StringConverter.h"

namespace SC
{
template <int N>
struct StringNative;
} // namespace SC

// Allows obtaining a null terminated char pointer to use with OS native api
template <int N>
struct SC::StringNative
{
#if SC_PLATFORM_WINDOWS
    typedef wchar_t CharType;
#else
    typedef char CharType;
#endif

    /// Returns a null terminated converted string
    const CharType* getText() const
    {
        // Probably forgot to call convertToNullTerminated
        SC_RELEASE_ASSERT(data != nullptr);
        return data;
    }

    /// Size of string obtained with getText() in bytes
    size_t getTextLengthInBytes() const { return dataSizeInBytes; }

    /// Size of string obtained with getText() in code points
    size_t getTextLengthInPoints() const { return dataSizeInBytes / sizeof(CharType); }

    void clear() { (void)buffer.resizeWithoutInitializing(0); }

    [[nodiscard]] bool convertToNullTerminated(StringView input)
    {
        SC_TRY_IF(buffer.resizeWithoutInitializing(0));
        dataSizeInBytes = 0;
        return appendNullTerminated(input, false); // forceCopy = true
    }
    /// Appends the input string null terminated
    [[nodiscard]] bool appendNullTerminated(StringView input, bool forceCopy)
    {
        if (!buffer.isEmpty())
        {
            SC_TRY_IF(buffer.pop_back());
        }
#if SC_PLATFORM_WINDOWS
        SC_TRY_IF(StringConverter::toNullTerminatedUTF16(input, buffer, &data, forceCopy));
#else
        SC_TRY_IF(StringConverter::toNullTerminatedUTF8(input, buffer, &data, forceCopy));
#endif
        if (buffer.size() == 0)
        {
            dataSizeInBytes = input.sizeInBytes();
        }
        else
        {
            dataSizeInBytes = buffer.size() * sizeof(CharType);
        }
        return true;
    }

    [[nodiscard]] bool setTextLengthInBytes(size_t newDataSize)
    {
        dataSizeInBytes = newDataSize;
        const bool res  = buffer.resize(newDataSize / sizeof(CharType) - 1);
        return res && buffer.push_back(0);
    }
#if SC_PLATFORM_WINDOWS
    StringView view() const
    {
        return StringView(reinterpret_cast<const char*>(buffer.data()), buffer.size() * sizeof(CharType), true,
                          StringEncoding::Utf16);
    }
#else
    StringView view() const
    {
        return StringView(buffer.data(), buffer.size() * sizeof(CharType), true, StringEncoding::Utf8);
    }
#endif
  private:
    SmallVector<CharType, N> buffer;
    const CharType*          data            = nullptr;
    size_t                   dataSizeInBytes = 0;
};
