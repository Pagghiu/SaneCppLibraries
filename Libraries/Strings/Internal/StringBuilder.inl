// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Result.h"
#include "../../Strings/String.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringConverter.h"

namespace SC
{
StringBuilder::StringBuilder(Buffer& stringData, StringEncoding encoding, Flags f)
    : stringData(stringData), encoding(encoding)
{
    if (f == Clear)
    {
        clear();
    }
}
StringBuilder::StringBuilder(String& str, Flags f) : stringData(str.data), encoding(str.getEncoding())
{
    if (f == Clear)
    {
        clear();
    }
}
bool StringBuilder::format(StringView text)
{
    clear();
    return append(text);
}

bool StringBuilder::append(StringView str)
{
    if (str.isEmpty())
        return true;
    (void)popNullTermIfNotEmpty(stringData, encoding);
    return StringConverter::convertEncodingTo(encoding, str, stringData);
}

bool StringBuilder::appendReplaceAll(StringView source, StringView occurrencesOf, StringView with)
{
    SC_ASSERT_RELEASE(occurrencesOf.hasCompatibleEncoding(with));

    if (source.isEmpty())
    {
        return true;
    }
    if (occurrencesOf.isEmpty())
    {
        return append(source);
    }
    StringView current = source;

    auto func = [&](auto sourceIt, auto occurrencesIt) -> bool
    {
        bool matches;
        do
        {
            sourceIt = current.getIterator<decltype(sourceIt)>();
            matches  = sourceIt.advanceBeforeFinding(occurrencesIt);
            SC_TRY(append(StringView::fromIteratorFromStart(sourceIt)));
            if (matches)
            {
                SC_TRY(append(with));
                sourceIt = current.getIterator<decltype(sourceIt)>();
                matches  = sourceIt.advanceAfterFinding(occurrencesIt); // TODO: advanceBeforeAndAfterFinding?
                current  = StringView::fromIteratorUntilEnd(sourceIt);
            }
        } while (matches);
        return true;
    };
    SC_TRY(StringView::withIterators(current, occurrencesOf, func));
    return append(current);
}

[[nodiscard]] bool StringBuilder::appendReplaceMultiple(StringView source, Span<const ReplacePair> substitutions)
{
    String buffer, other;
    SC_TRY(buffer.assign(source));
    for (auto it : substitutions)
    {
        if (it.searchFor == it.replaceWith)
            continue;
        StringBuilder sb(other, StringBuilder::Clear);
        SC_TRY(sb.appendReplaceAll(buffer.view(), it.searchFor, it.replaceWith));
        swap(other, buffer);
    }
    return append(buffer.view());
}

bool StringBuilder::appendHex(Span<const uint8_t> data, AppendHexCase casing)
{
    if (encoding == StringEncoding::Utf16)
        return false; // TODO: Support appendHex for UTF16
    const size_t previousSize = stringData.size();
    SC_TRY(stringData.resizeWithoutInitializing(stringData.size() + data.sizeInBytes() * 2));
    const size_t   sizeInBytes = data.sizeInBytes();
    const uint8_t* sourceBytes = data.data();
    char*          destination = stringData.data();
    for (size_t idx = 0; idx < sizeInBytes; idx++)
    {
        constexpr char bytesUpper[] = "0123456789ABCDEF";
        constexpr char bytesLower[] = "0123456789abcdef";
        switch (casing)
        {
        case AppendHexCase::UpperCase:
            destination[previousSize + idx * 2 + 0] = bytesUpper[sourceBytes[idx] >> 4];
            destination[previousSize + idx * 2 + 1] = bytesUpper[sourceBytes[idx] & 0x0F];
            break;
        case AppendHexCase::LowerCase:
            destination[previousSize + idx * 2 + 0] = bytesLower[sourceBytes[idx] >> 4];
            destination[previousSize + idx * 2 + 1] = bytesLower[sourceBytes[idx] & 0x0F];
            break;
        }
    }
    return pushNullTerm(stringData, encoding);
}

void StringBuilder::clear() { stringData.clear(); }

bool StringBuilder::popNullTermIfNotEmpty(Buffer& stringData, StringEncoding encoding)
{
    const auto sizeOfZero = StringEncodingGetSize(encoding);
    const auto dataSize   = stringData.size();
    if (dataSize >= sizeOfZero)
    {
        (void)stringData.resizeWithoutInitializing(dataSize - sizeOfZero);
        return true;
    }
    else
    {
        return false;
    }
}

bool StringBuilder::pushNullTerm(Buffer& stringData, StringEncoding encoding)
{
    return stringData.resize(stringData.size() + StringEncodingGetSize(encoding), 0);
}

} // namespace SC
