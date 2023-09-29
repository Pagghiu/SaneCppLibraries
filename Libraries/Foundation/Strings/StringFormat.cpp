// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringFormat.h"
#include "../../System/Console.h" // TODO: Console here is a module circular dependency. Consider type-erasing with a Function
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
    char       buffer[BUFFER_SIZE];
    const int  numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), compoundSpecifier, value);
    const bool validResult =
        (numCharsExcludingTerminator >= 0) and (static_cast<size_t>(numCharsExcludingTerminator + 1) < BUFFER_SIZE);
    return validResult && data.write(StringView(buffer, static_cast<size_t>(numCharsExcludingTerminator), true,
                                                StringEncoding::Ascii));
}
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#else

bool StringFormatterFor<SC::size_t>::format(StringFormatOutput& data, const StringView specifier,
                                            const SC::size_t value)
{
    constexpr char formatSpecifier[] = "zu";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::ssize_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::ssize_t value)
{
    constexpr char formatSpecifier[] = "zd";
    return formatSprintf(data, formatSpecifier, specifier, value);
}
#endif

bool StringFormatterFor<SC::int64_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::int64_t value)
{
    constexpr char formatSpecifier[] = PRIi64;
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint64_t>::format(StringFormatOutput& data, const StringView specifier,
                                              const SC::uint64_t value)
{
    constexpr char formatSpecifier[] = PRIu64;
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::int32_t>::format(StringFormatOutput& data, const StringView specifier,
                                             const SC::int32_t value)
{
    constexpr char formatSpecifier[] = "d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint32_t>::format(StringFormatOutput& data, const StringView specifier,
                                              const SC::uint32_t value)
{
    constexpr char formatSpecifier[] = "d";
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

bool StringFormatterFor<bool>::format(StringFormatOutput& data, const StringView specifier, const bool value)
{
    SC_UNUSED(specifier);
    return data.write(value ? "true"_a8 : "false"_a8);
}
bool StringFormatterFor<float>::format(StringFormatOutput& data, StringView specifier, const float value)
{
    constexpr char formatSpecifier[] = "f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<double>::format(StringFormatOutput& data, const StringView specifier, const double value)
{
    constexpr char formatSpecifier[] = "f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<char>::format(StringFormatOutput& data, const StringView specifier, const char value)
{
    SC_UNUSED(specifier);
    return data.write(StringView(&value, sizeof(value), false, StringEncoding::Ascii));
}

bool StringFormatterFor<const char*>::format(StringFormatOutput& data, const StringView specifier, const char* value)
{
    SC_UNUSED(specifier);
    return data.write(StringView(value, strlen(value), true, StringEncoding::Ascii));
}

#if SC_PLATFORM_WINDOWS
bool StringFormatterFor<wchar_t>::format(StringFormatOutput& data, const StringView specifier, const wchar_t value)
{
    SC_UNUSED(specifier);
    return data.write(StringView({&value, sizeof(value)}, false));
}

bool StringFormatterFor<const wchar_t*>::format(StringFormatOutput& data, const StringView specifier,
                                                const wchar_t* value)
{
    SC_UNUSED(specifier);
    return data.write(StringView({value, wcslen(value) * sizeof(wchar_t)}, true));
}
#endif

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

} // namespace SC
