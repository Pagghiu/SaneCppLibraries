// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringFormat.h"
#include "../System/Console.h" // TODO: Console here is a module circular dependency. Consider type-erasing with a Function
#include "StringBuilder.h"
#include "StringConverter.h"

#include <inttypes.h> // PRIu64 / PRIi64
#include <stdio.h>    // snprintf
#include <string.h>   // strlen
#include <wchar.h>    // wcslen

namespace SC
{

template <size_t BUFFER_SIZE = 100, size_t FORMAT_LENGTH, typename Value>
bool formatSprintf(StringFormatOutput& data, const char (&formatSpecifier)[FORMAT_LENGTH], StringView specifier,
                   const Value value)
{
    const size_t SPECIFIER_SIZE = 50;
    char         compoundSpecifier[SPECIFIER_SIZE];
    compoundSpecifier[0]         = '%';
    const size_t specifierLength = specifier.sizeInBytes();
    if (specifierLength > sizeof(compoundSpecifier) - FORMAT_LENGTH - 2)
        return false; // if someone things it's a good idea doing a specifier > 50 chars...
    memcpy(compoundSpecifier + 1, specifier.bytesWithoutTerminator(), specifierLength);
    memcpy(compoundSpecifier + 1 + specifierLength, formatSpecifier, FORMAT_LENGTH);
    compoundSpecifier[1 + specifierLength + FORMAT_LENGTH] = 0;
    char_t     buffer[BUFFER_SIZE];
    const int  numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), compoundSpecifier, value);
    const bool validResult =
        (numCharsExcludingTerminator >= 0) and (static_cast<size_t>(numCharsExcludingTerminator + 1) < BUFFER_SIZE);
    return validResult && data.write(StringView(buffer, static_cast<size_t>(numCharsExcludingTerminator), true,
                                                StringEncoding::Ascii));
}
#if SC_MSVC || SC_CLANG_CL
#else

bool StringFormatterFor<SC::size_t>::format(StringFormatOutput& data, const StringView specifier,
                                            const SC::size_t value)
{
    constexpr char_t formatSpecifier[] = "zu";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::ssize_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::ssize_t value)
{
    constexpr char_t formatSpecifier[] = "zd";
    return formatSprintf(data, formatSpecifier, specifier, value);
}
#endif

bool StringFormatterFor<SC::int64_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::int64_t value)
{
    constexpr char_t formatSpecifier[] = PRIi64;
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint64_t>::format(StringFormatOutput& data, const StringView specifier,
                                              const SC::uint64_t value)
{
    constexpr char_t formatSpecifier[] = PRIu64;
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::int32_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::int32_t value)
{
    constexpr char_t formatSpecifier[] = "d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint32_t>::format(StringFormatOutput& data, const StringView specifier,
                                              const SC::uint32_t value)
{
    constexpr char_t formatSpecifier[] = "d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::int16_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::int16_t value)
{
    return StringFormatterFor<SC::int32_t>::format(data, specifier, value);
}

bool StringFormatterFor<SC::uint16_t>::format(StringFormatOutput& data, const StringView specifier,
                                              const SC::uint16_t value)
{
    return StringFormatterFor<SC::uint32_t>::format(data, specifier, value);
}
bool StringFormatterFor<SC::int8_t>::format(StringFormatOutput& data, const StringView specifier,
                                            const SC::int8_t value)
{
    return StringFormatterFor<SC::int32_t>::format(data, specifier, value);
}

bool StringFormatterFor<SC::uint8_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::uint8_t value)
{
    return StringFormatterFor<SC::uint32_t>::format(data, specifier, value);
}

bool StringFormatterFor<float>::format(StringFormatOutput& data, StringView specifier, const float value)
{
    constexpr char_t formatSpecifier[] = "f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<double>::format(StringFormatOutput& data, const StringView specifier, const double value)
{
    constexpr char_t formatSpecifier[] = "f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::char_t>::format(StringFormatOutput& data, const StringView specifier,
                                            const SC::char_t value)
{
    SC_UNUSED(specifier);
    return data.write(StringView(&value, sizeof(value), false, StringEncoding::Ascii));
}

bool StringFormatterFor<wchar_t>::format(StringFormatOutput& data, const StringView specifier, const wchar_t value)
{
    SC_UNUSED(specifier);
    return data.write(StringView({&value, sizeof(value)}, false, StringEncoding::Utf16));
}

bool StringFormatterFor<const SC::char_t*>::format(StringFormatOutput& data, const StringView specifier,
                                                   const SC::char_t* value)
{
    SC_UNUSED(specifier);
    return data.write(StringView(value, strlen(value), true, StringEncoding::Ascii));
}

bool StringFormatterFor<const wchar_t*>::format(StringFormatOutput& data, const StringView specifier,
                                                const wchar_t* value)
{
    SC_UNUSED(specifier);
    return data.write(StringView({value, wcslen(value) * sizeof(wchar_t)}, true, StringEncoding::Utf16));
}

bool StringFormatterFor<SC::StringView>::format(StringFormatOutput& data, const StringView specifier,
                                                const SC::StringView value)
{
    SC_UNUSED(specifier);
    return data.write(value);
}

bool StringFormatOutput::write(StringView text)
{
    if (text.isEmpty())
    {
        return true;
    }
    if (console != nullptr)
    {
        console->print(text);
        return true;
    }
    else if (data != nullptr)
    {
        if (StringEncodingAreBinaryCompatible(encoding, text.getEncoding()))
        {
            return data->appendCopy(text.bytesWithoutTerminator(), text.sizeInBytes());
        }
        else
        {
            return StringConverter::convertEncodingTo(encoding, text, *data, nullptr,
                                                      StringConverter::DoNotAddZeroTerminator);
        }
    }
    else
    {
        SC_DEBUG_ASSERT("StringFormatOutput::write - Forgot to set buffer or console" && 0);
        return false;
    }
}

void StringFormatOutput::redirectToBuffer(Vector<char>& destination)
{
    data    = &destination;
    console = nullptr;
}

void StringFormatOutput::redirectToConsole(Console& newConsole)
{
    data    = nullptr;
    console = &newConsole;
}

void StringFormatOutput::onFormatBegin()
{
    if (data != nullptr)
    {
        backupSize = data->size();
    }
}

bool StringFormatOutput::onFormatSucceded()
{
    if (data != nullptr)
    {
        if (backupSize < data->size())
        {
            // Add null terminator
            return data->resize(data->size() + StringEncodingGetSize(encoding));
        }
    }
    return true;
}

void StringFormatOutput::onFormatFailed()
{
    if (data != nullptr)
    {
        SC_TRUST_RESULT(data->resize(backupSize));
    }
}

bool StringBuilder::append(StringView str)
{
    if (str.isEmpty())
        return true;
    SC_TRY_IF(backingString.popNulltermIfExists());
    return StringConverter::convertEncodingTo(backingString.getEncoding(), str, backingString.data);
}

bool StringBuilder::appendReplaceAll(StringView source, StringView occurrencesOf, StringView with)
{
    if (not source.hasCompatibleEncoding(occurrencesOf) or not source.hasCompatibleEncoding(with) or
        not source.hasCompatibleEncoding(backingString.view()))
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
    SC_TRY_IF(backingString.popNulltermIfExists());
    StringView current             = source;
    const auto occurrencesIterator = occurrencesOf.getIterator<StringIteratorASCII>();
    bool       res;
    do
    {
        auto sourceIt    = current.getIterator<StringIteratorASCII>();
        res              = sourceIt.advanceBeforeFinding(occurrencesIterator);
        StringView soFar = StringView::fromIteratorFromStart(sourceIt);
        SC_TRY_IF(backingString.data.appendCopy(soFar.bytesWithoutTerminator(), soFar.sizeInBytes()));
        if (res)
        {
            SC_TRY_IF(backingString.data.appendCopy(with.bytesWithoutTerminator(), with.sizeInBytes()));
            res     = sourceIt.advanceByLengthOf(occurrencesIterator);
            current = StringView::fromIteratorUntilEnd(sourceIt);
        }
    } while (res);
    SC_TRY_IF(backingString.data.appendCopy(current.bytesWithoutTerminator(), current.sizeInBytes()));
    return backingString.pushNullTerm();
}

[[nodiscard]] bool StringBuilder::appendReplaceMultiple(StringView source, Span<const StringView[2]> substitutions)
{
    String buffer, other;
    SC_TRY_IF(buffer.assign(source));
    for (auto it : substitutions)
    {
        SC_TRY_IF(StringBuilder(other, StringBuilder::Clear).appendReplaceAll(buffer.view(), it[0], it[1]));
        swap(other, buffer);
    }
    return append(buffer.view());
}

bool StringBuilder::appendHex(SpanVoid<const void> data)
{
    const unsigned char* bytes = data.castTo<const unsigned char>().data();
    if (backingString.encoding == StringEncoding::Utf16)
        return false; // TODO: Support appendHex for UTF16
    const auto oldSize = backingString.data.size();
    SC_TRY_IF(backingString.data.resizeWithoutInitializing(backingString.data.size() + data.sizeInBytes() * 2));
    const auto sizeInBytes = data.sizeInBytes();
    for (size_t i = 0; i < sizeInBytes; i++)
    {
        backingString.data[oldSize + i * 2]     = "0123456789ABCDEF"[bytes[i] >> 4];
        backingString.data[oldSize + i * 2 + 1] = "0123456789ABCDEF"[bytes[i] & 0x0F];
    }
    return backingString.pushNullTerm();
}

} // namespace SC
