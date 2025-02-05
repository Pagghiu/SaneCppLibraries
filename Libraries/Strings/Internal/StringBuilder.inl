// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Result.h"
#include "../../Strings/String.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringConverter.h"

namespace SC
{
StringBuilder::StringBuilder(Vector<char>& stringData, StringEncoding encoding, Flags f)
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
    (void)StringConverter::popNullTermIfNotEmpty(stringData, encoding);
    return StringConverter::convertEncodingTo(encoding, str, stringData);
}

bool StringBuilder::appendReplaceAll(StringView source, StringView occurrencesOf, StringView with)
{
    if (not source.hasCompatibleEncoding(occurrencesOf) or not source.hasCompatibleEncoding(with) or
        not StringEncodingAreBinaryCompatible(source.getEncoding(), encoding))
    {
        return false;
    }
    if (source.isEmpty())
    {
        return true;
    }
    if (occurrencesOf.isEmpty())
    {
        return append(source);
    }
    (void)StringConverter::popNullTermIfNotEmpty(stringData, encoding);
    StringView current             = source;
    const auto occurrencesIterator = occurrencesOf.getIterator<StringIteratorASCII>();
    bool       res;
    do
    {
        auto sourceIt    = current.getIterator<StringIteratorASCII>();
        res              = sourceIt.advanceBeforeFinding(occurrencesIterator);
        StringView soFar = StringView::fromIteratorFromStart(sourceIt);
        SC_TRY(stringData.append(soFar.toCharSpan()));
        if (res)
        {
            SC_TRY(stringData.append(with.toCharSpan()));
            res     = sourceIt.advanceByLengthOf(occurrencesIterator);
            current = StringView::fromIteratorUntilEnd(sourceIt);
        }
    } while (res);
    SC_TRY(stringData.append(current.toCharSpan()));
    return StringConverter::pushNullTerm(stringData, encoding);
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
    for (size_t idx = 0; idx < sizeInBytes; idx++)
    {
        constexpr char bytesUpper[] = "0123456789ABCDEF";
        constexpr char bytesLower[] = "0123456789abcdef";
        switch (casing)
        {
        case AppendHexCase::UpperCase:
            stringData[previousSize + idx * 2 + 0] = bytesUpper[sourceBytes[idx] >> 4];
            stringData[previousSize + idx * 2 + 1] = bytesUpper[sourceBytes[idx] & 0x0F];
            break;
        case AppendHexCase::LowerCase:
            stringData[previousSize + idx * 2 + 0] = bytesLower[sourceBytes[idx] >> 4];
            stringData[previousSize + idx * 2 + 1] = bytesLower[sourceBytes[idx] & 0x0F];
            break;
        }
    }
    return StringConverter::pushNullTerm(stringData, encoding);
}

void StringBuilder::clear() { stringData.clearWithoutInitializing(); }

} // namespace SC
