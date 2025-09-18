// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Result.h"
#include "../../Strings/String.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringConverter.h"

namespace SC
{

StringBuilder::StringBuilder(IGrowableBuffer& bufferT, StringEncoding encoding, Flags flags)
{
    destroyBuffer = false;
    initWithEncoding(bufferT, encoding, flags);
}

void StringBuilder::initWithEncoding(IGrowableBuffer& ibuffer, StringEncoding stringEncoding, Flags flags)
{
    encoding = stringEncoding;
    buffer   = &ibuffer;
    if (flags == Clear)
    {
        clear();
    }
}

StringBuilder::~StringBuilder() { finalize(); }

StringView StringBuilder::finalize()
{
    if (buffer)
    {
        (void)popNullTermIfNotEmpty(*buffer, encoding);
        if (destroyBuffer)
        {
            buffer->~IGrowableBuffer();
        }
        finalizedView = {{buffer->data(), buffer->size()}, true, encoding};
        buffer        = nullptr;
    }
    return finalizedView;
}

bool StringBuilder::append(StringView str)
{
    if (buffer == nullptr)
        return false;
    if (str.isEmpty())
        return true;
    (void)popNullTermIfNotEmpty(*buffer, encoding);
    return StringConverter::appendEncodingTo(encoding, str, *buffer, StringConverter::NullTerminate);
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

[[nodiscard]] bool StringBuilder::appendReplaceMultiple(StringView source, Span<const ReplacePair> substitutions)
{
    if (buffer == nullptr)
        return false;
    String tempBuffer, other;
    SC_TRY(tempBuffer.assign(source));
    for (auto it : substitutions)
    {
        if (it.searchFor == it.replaceWith)
            continue;
        StringBuilder sb(other, StringBuilder::Clear);
        SC_TRY(sb.appendReplaceAll(tempBuffer.view(), it.searchFor, it.replaceWith));
        sb.finalize();
        swap(other, tempBuffer);
    }
    return append(tempBuffer.view());
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
    return pushNullTerm(*buffer, encoding);
}

void StringBuilder::clear()
{
    if (buffer)
        buffer->clear();
}

bool StringBuilder::popNullTermIfNotEmpty(IGrowableBuffer& buffer, StringEncoding encoding)
{
    const auto sizeOfZero = StringEncodingGetSize(encoding);
    const auto dataSize   = buffer.size();
    if (dataSize >= sizeOfZero)
    {
        (void)buffer.resizeWithoutInitializing(dataSize - sizeOfZero);
        return true;
    }
    else
    {
        return false;
    }
}

bool StringBuilder::pushNullTerm(IGrowableBuffer& buffer, StringEncoding encoding)
{
    const size_t oldSize = buffer.size();
    SC_TRY(buffer.resizeWithoutInitializing(buffer.size() + StringEncodingGetSize(encoding)));
    ::memset(buffer.data() + oldSize, 0, StringEncodingGetSize(encoding));
    return true;
}

} // namespace SC
