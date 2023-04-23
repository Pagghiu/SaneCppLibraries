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

template <size_t BUFFER_SIZE = 100, size_t SPECIFIER_LENGTH, typename Value>
bool formatSprintf(StringFormatOutput& data, const char (&formatSpecifier)[SPECIFIER_LENGTH],
                   StringIteratorASCII specifier, const Value value)
{
    const int SPECIFIER_SIZE = 50;
    char      compoundSpecifier[SPECIFIER_SIZE];
    compoundSpecifier[0]   = '%';
    size_t specifierLength = specifier.getEnd() - specifier.getIt();
    memcpy(compoundSpecifier + 1, specifier.getIt(), specifierLength);
    memcpy(compoundSpecifier + 1 + specifierLength, formatSpecifier, SPECIFIER_LENGTH);
    compoundSpecifier[1 + specifierLength + SPECIFIER_LENGTH] = 0;
    char_t     buffer[BUFFER_SIZE];
    const int  numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), compoundSpecifier, value);
    const bool validResult = numCharsExcludingTerminator >= 0 && numCharsExcludingTerminator + 1 < BUFFER_SIZE;
    return validResult && data.write(StringView(buffer, numCharsExcludingTerminator, true, StringEncoding::Ascii));
}
#if SC_MSVC
#else

bool StringFormatterFor<SC::size_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                            const SC::size_t value)
{
    constexpr char_t formatSpecifier[] = "zu";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::ssize_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                             const SC::ssize_t value)
{
    constexpr char_t formatSpecifier[] = "zd";
    return formatSprintf(data, formatSpecifier, specifier, value);
}
#endif

bool StringFormatterFor<SC::int64_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                             const SC::int64_t value)
{
    constexpr char_t formatSpecifier[] = PRIi64;
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint64_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                              const SC::uint64_t value)
{
    constexpr char_t formatSpecifier[] = PRIu64;
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::int32_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                             const SC::int32_t value)
{
    constexpr char_t formatSpecifier[] = "d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint32_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                              const SC::uint32_t value)
{
    constexpr char_t formatSpecifier[] = "d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::int16_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                             const SC::int16_t value)
{
    return StringFormatterFor<SC::int32_t>::format(data, specifier, value);
}

bool StringFormatterFor<SC::uint16_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                              const SC::uint16_t value)
{
    return StringFormatterFor<SC::uint32_t>::format(data, specifier, value);
}
bool StringFormatterFor<SC::int8_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                            const SC::int8_t value)
{
    return StringFormatterFor<SC::int32_t>::format(data, specifier, value);
}

bool StringFormatterFor<SC::uint8_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                             const SC::uint8_t value)
{
    return StringFormatterFor<SC::uint32_t>::format(data, specifier, value);
}

bool StringFormatterFor<float>::format(StringFormatOutput& data, StringIteratorASCII specifier, const float value)
{
    constexpr char_t formatSpecifier[] = "f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<double>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                        const double value)
{
    constexpr char_t formatSpecifier[] = "f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::char_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                            const SC::char_t value)
{
    SC_UNUSED(specifier);
    return data.write(StringView(&value, sizeof(value), false, StringEncoding::Ascii));
}

bool StringFormatterFor<wchar_t>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                         const wchar_t value)
{
    SC_UNUSED(specifier);
    return data.write(StringView({&value, sizeof(value)}, false, StringEncoding::Utf16));
}

bool StringFormatterFor<const SC::char_t*>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                                   const SC::char_t* value)
{
    SC_UNUSED(specifier);
    return data.write(StringView(value, strlen(value), true, StringEncoding::Ascii));
}

bool StringFormatterFor<const wchar_t*>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                                const wchar_t* value)
{
    SC_UNUSED(specifier);
    return data.write(StringView({value, wcslen(value) * sizeof(wchar_t)}, true, StringEncoding::Utf16));
}

bool StringFormatterFor<SC::StringView>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
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
        (void)data->resize(backupSize);
    }
}

bool StringBuilder::append(StringView str)
{
    if (str.isEmpty())
        return true;
    SC_TRY_IF(backingString.popNulltermIfExists());
    return StringConverter::convertEncodingTo(backingString.getEncoding(), str, backingString.data);
}

} // namespace SC
