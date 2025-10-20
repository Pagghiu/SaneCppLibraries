// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Result.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringConverter.h"

namespace SC
{

StringBuilder::StringBuilder(IGrowableBuffer& ibuffer, StringEncoding encoding, Flags flags) noexcept
{
    initWithEncoding(ibuffer, encoding, flags);
}

void StringBuilder::initWithEncoding(IGrowableBuffer& ibuffer, StringEncoding stringEncoding, Flags flags) noexcept
{
    encoding = stringEncoding;
    buffer   = &ibuffer;
    if (flags == Clear)
    {
        if (buffer)
            buffer->clear();
    }
}

bool StringBuilder::append(StringView str)
{
    if (buffer == nullptr)
        return false;
    if (str.isEmpty())
        return true;
    return StringConverter::appendEncodingTo(encoding, str, *buffer, StringConverter::DoNotTerminate);
}

bool StringBuilder::appendReplaceAll(StringView source, StringView occurrencesOf, StringView with)
{
    if (buffer == nullptr)
        return false;
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

bool StringBuilder::appendHex(Span<const uint8_t> data, AppendHexCase casing)
{
    if (buffer == nullptr)
        return false;
    if (encoding == StringEncoding::Utf16)
        return false; // TODO: Support appendHex for UTF16
    const size_t previousSize = buffer->size();
    SC_TRY(buffer->resizeWithoutInitializing(buffer->size() + data.sizeInBytes() * 2));
    const size_t   sizeInBytes = data.sizeInBytes();
    const uint8_t* sourceBytes = data.data();
    char*          destination = buffer->data();
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
    return true;
}

} // namespace SC
