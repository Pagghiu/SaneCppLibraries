// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/StringPath.h"

namespace SC
{
namespace PluginString
{
[[nodiscard]] constexpr StringSpan slice(StringSpan text, size_t start, size_t length)
{
    return {{text.bytesWithoutTerminator() + start, length}, false, text.getEncoding()};
}

[[nodiscard]] constexpr bool endsWith(StringSpan text, StringSpan suffix)
{
    const bool compatible =
        text.getEncoding() == suffix.getEncoding() or
        (text.getEncoding() != StringEncoding::Utf16 and suffix.getEncoding() != StringEncoding::Utf16);
    return compatible and text.sizeInBytes() >= suffix.sizeInBytes() and
           slice(text, text.sizeInBytes() - suffix.sizeInBytes(), suffix.sizeInBytes()) == suffix;
}

[[nodiscard]] inline bool pathEndsWith(StringSpan path, StringSpan suffix)
{
    size_t pathOffset   = path.sizeInBytes();
    size_t suffixOffset = suffix.sizeInBytes();

    const auto readUnitBack = [](StringSpan text, size_t& offset, const char*& unit, size_t& unitSize)
    {
        unitSize = StringEncodingGetSize(text.getEncoding());
        if (offset < unitSize)
            return false;
        offset -= unitSize;
        unit = text.bytesWithoutTerminator() + offset;
        return true;
    };

    const auto isSameUnit = [](const char* left, size_t leftSize, const char* right, size_t rightSize)
    {
        if (leftSize != rightSize)
            return false;
        for (size_t idx = 0; idx < leftSize; ++idx)
        {
#if SC_PLATFORM_WINDOWS
            const auto lowerCaseASCII = [](char character)
            { return character >= 'A' and character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character; };
            if (lowerCaseASCII(left[idx]) != lowerCaseASCII(right[idx]))
#else
            if (left[idx] != right[idx])
#endif
                return false;
        }
        return true;
    };

    const auto readAscii = [](const char* unit, size_t unitSize, char& character)
    {
        if (unitSize == 2 and unit[1] != 0)
            return false;
        character = unit[0];
        return true;
    };

    const auto isSeparator = [](char character) { return character == '/' or character == '\\'; };

    while (suffixOffset > 0)
    {
        const char* pathUnit   = nullptr;
        const char* suffixUnit = nullptr;
        size_t      pathSize   = 0;
        size_t      suffixSize = 0;
        if (not readUnitBack(path, pathOffset, pathUnit, pathSize) or
            not readUnitBack(suffix, suffixOffset, suffixUnit, suffixSize))
        {
            return false;
        }
        if (isSameUnit(pathUnit, pathSize, suffixUnit, suffixSize))
            continue;

        char pathCharacter   = 0;
        char suffixCharacter = 0;
        if (not readAscii(pathUnit, pathSize, pathCharacter) or not readAscii(suffixUnit, suffixSize, suffixCharacter))
        {
            return false;
        }
        if (isSeparator(pathCharacter) and isSeparator(suffixCharacter))
            continue;
        return false;
    }
    return true;
}

[[nodiscard]] constexpr bool find(StringSpan text, StringSpan needle, size_t from, size_t& position)
{
    if (text.getEncoding() == StringEncoding::Utf16 or needle.getEncoding() == StringEncoding::Utf16 or
        needle.sizeInBytes() > text.sizeInBytes())
        return false;
    for (size_t idx = from; idx + needle.sizeInBytes() <= text.sizeInBytes(); ++idx)
    {
        if (slice(text, idx, needle.sizeInBytes()) == needle)
        {
            position = idx;
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr StringSpan trimEnd(StringSpan text, char character)
{
    const size_t unitSize = StringEncodingGetSize(text.getEncoding());
    size_t       size     = text.sizeInBytes();
    while (size >= unitSize)
    {
        const char* current = text.bytesWithoutTerminator() + size - unitSize;
        if (current[0] != character or (unitSize == 2 and current[1] != 0))
            break;
        size -= unitSize;
    }
    return slice(text, 0, size);
}

[[nodiscard]] constexpr StringSpan trimEndNewLines(StringSpan text)
{
    StringSpan trimmed = trimEnd(text, '\n');
    return trimEnd(trimmed, '\r');
}

[[nodiscard]] constexpr size_t findLastSeparator(StringSpan path)
{
    const size_t unitSize = StringEncodingGetSize(path.getEncoding());
    size_t       position = path.sizeInBytes();
    while (position >= unitSize)
    {
        position -= unitSize;
        const char* current = path.bytesWithoutTerminator() + position;
        if ((current[0] == '/' or current[0] == '\\') and (unitSize == 1 or current[1] == 0))
            return position;
    }
    return path.sizeInBytes();
}

[[nodiscard]] constexpr StringSpan basename(StringSpan path)
{
    const size_t separator = findLastSeparator(path);
    const size_t unitSize  = StringEncodingGetSize(path.getEncoding());
    return separator == path.sizeInBytes()
               ? path
               : slice(path, separator + unitSize, path.sizeInBytes() - separator - unitSize);
}

[[nodiscard]] constexpr StringSpan basename(StringSpan path, StringSpan suffix)
{
    StringSpan base = basename(path);
    return endsWith(base, suffix) ? slice(base, 0, base.sizeInBytes() - suffix.sizeInBytes()) : base;
}

[[nodiscard]] constexpr StringSpan dirname(StringSpan path)
{
    const size_t separator = findLastSeparator(path);
    return separator == path.sizeInBytes() ? StringSpan(path.getEncoding()) : slice(path, 0, separator);
}

#if SC_PLATFORM_WINDOWS
[[nodiscard]] constexpr StringSpan withoutWindowsRoot(StringSpan path)
{
    if (path.getEncoding() != StringEncoding::Utf16)
        return path;
    const size_t units = path.sizeInBytes() / sizeof(wchar_t);
    const auto   at    = [&](size_t index) -> char
    { return index < units ? path.bytesWithoutTerminator()[index * sizeof(wchar_t)] : 0; };
    const auto isSeparator = [&](size_t index) { return at(index) == '\\' or at(index) == '/'; };
    size_t     rootUnits   = 0;
    if (units >= 3 and at(1) == ':' and isSeparator(2))
    {
        rootUnits = 3;
    }
    else if (units >= 2 and isSeparator(0) and isSeparator(1))
    {
        size_t index = 2;
        if (units >= 4 and at(2) == '?' and isSeparator(3))
        {
            index = 4;
            if (units >= 7 and at(5) == ':' and isSeparator(6))
                rootUnits = 7;
        }
        if (rootUnits == 0)
        {
            while (index < units and not isSeparator(index))
                ++index;
            if (index < units)
                ++index;
            while (index < units and not isSeparator(index))
                ++index;
            rootUnits = index < units ? index + 1 : index;
        }
    }
    else if (units > 0 and isSeparator(0))
    {
        rootUnits = 1;
    }
    return slice(path, rootUnits * sizeof(wchar_t), path.sizeInBytes() - rootUnits * sizeof(wchar_t));
}
#endif

[[nodiscard]] inline bool append(StringPath& output, Span<const StringSpan> components)
{
    for (StringSpan component : components)
    {
        SC_TRY(output.append(component));
    }
    return true;
}

[[nodiscard]] inline bool assign(StringPath& output, Span<const StringSpan> components)
{
    SC_TRY(output.resize(0));
    return append(output, components);
}

[[nodiscard]] inline bool join(StringPath& output, Span<const StringSpan> components)
{
    SC_TRY(output.resize(0));
    bool first = true;
    for (StringSpan component : components)
    {
        if (component.isEmpty())
            continue;
        if (not first)
            SC_TRY(output.append(SC_NATIVE_STR("/")));
        SC_TRY(output.append(component));
        first = false;
    }
    return true;
}

struct Tokenizer
{
    StringSpan text;
    StringSpan component;
    size_t     offset = 0;

    explicit constexpr Tokenizer(StringSpan text) : text(text) {}

    [[nodiscard]] constexpr bool next(char delimiter, bool skipEmpty = true)
    {
        const size_t unitSize = StringEncodingGetSize(text.getEncoding());
        while (offset <= text.sizeInBytes())
        {
            const size_t start = offset;
            while (offset < text.sizeInBytes())
            {
                const char* current = text.bytesWithoutTerminator() + offset;
                if (current[0] == delimiter and (unitSize == 1 or current[1] == 0))
                    break;
                offset += unitSize;
            }
            component = slice(text, start, offset - start);
            offset += unitSize;
            if (not skipEmpty or not component.isEmpty())
                return true;
        }
        return false;
    }
};

[[nodiscard]] constexpr bool parseUnsignedByte(StringSpan text, unsigned char& value)
{
    if (text.isEmpty())
        return false;
    unsigned int result   = 0;
    const size_t unitSize = StringEncodingGetSize(text.getEncoding());
    for (size_t offset = 0; offset < text.sizeInBytes(); offset += unitSize)
    {
        const char* current = text.bytesWithoutTerminator() + offset;
        if (unitSize == 2 and current[1] != 0)
            return false;
        const char character = current[0];
        if (character < '0' or character > '9')
            return false;
        result = result * 10 + static_cast<unsigned int>(character - '0');
        if (result > 255)
            return false;
    }
    value = static_cast<unsigned char>(result);
    return true;
}
} // namespace PluginString
} // namespace SC
