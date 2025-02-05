// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Strings/Console.h" // TODO: Console here is a module circular dependency. Consider type-erasing with a Function
#include "../../Strings/String.h"
#include "../../Strings/StringConverter.h"
#include "../../Strings/StringFormat.h"

#include <inttypes.h> // PRIu64 / PRIi64
#include <stdio.h>    // snprintf
#include <string.h>   // strlen
#include <wchar.h>    // wcslen

namespace SC
{

template <size_t BUFFER_SIZE = 100, size_t FORMAT_LENGTH, typename Value>
static bool formatSprintf(StringFormatOutput& data, const char (&formatSpecifier)[FORMAT_LENGTH], StringView specifier,
                          const Value value)
{
    const size_t SPECIFIER_SIZE = 50;
    char         compoundSpecifier[SPECIFIER_SIZE];
    compoundSpecifier[0]         = '%';
    const size_t specifierLength = specifier.sizeInBytes();
    if (specifierLength > sizeof(compoundSpecifier) - FORMAT_LENGTH - 2)
        return false; // if someone things it's a good idea doing a specifier > 50 chars...
    if (specifierLength > 0)
    {
        memcpy(compoundSpecifier + 1, specifier.bytesWithoutTerminator(), specifierLength);
    }
    memcpy(compoundSpecifier + 1 + specifierLength, formatSpecifier, FORMAT_LENGTH);
    compoundSpecifier[1 + specifierLength + FORMAT_LENGTH] = 0;
    char       buffer[BUFFER_SIZE];
    const int  numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), compoundSpecifier, value);
    const bool validResult =
        (numCharsExcludingTerminator >= 0) and (static_cast<size_t>(numCharsExcludingTerminator + 1) < BUFFER_SIZE);
    return validResult && data.append(StringView({buffer, static_cast<size_t>(numCharsExcludingTerminator)}, true,
                                                 StringEncoding::Ascii));
}
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#if SC_PLATFORM_64_BIT == 0
bool StringFormatterFor<ssize_t>::format(StringFormatOutput& data, const StringView specifier, const long value)
{
    constexpr char formatSpecifier[] = "d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}
#endif
#else
#if !SC_PLATFORM_LINUX
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
    SC_COMPILER_UNUSED(specifier);
    return data.append(value ? "true"_a8 : "false"_a8);
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
    SC_COMPILER_UNUSED(specifier);
    return data.append(StringView({&value, sizeof(value)}, false, StringEncoding::Ascii));
}

bool StringFormatterFor<const char*>::format(StringFormatOutput& data, const StringView specifier, const char* value)
{
    SC_COMPILER_UNUSED(specifier);
    return data.append(StringView::fromNullTerminated(value, StringEncoding::Ascii));
}

bool StringFormatterFor<const void*>::format(StringFormatOutput& data, const StringView specifier, const void* value)
{
    constexpr char formatSpecifier[] = "p";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

#if SC_PLATFORM_WINDOWS
bool StringFormatterFor<wchar_t>::format(StringFormatOutput& data, const StringView specifier, const wchar_t value)
{
    SC_COMPILER_UNUSED(specifier);
    return data.append(StringView({&value, 1}, false));
}

bool StringFormatterFor<const wchar_t*>::format(StringFormatOutput& data, const StringView specifier,
                                                const wchar_t* value)
{
    SC_COMPILER_UNUSED(specifier);
    return data.append(StringView({value, wcslen(value)}, true));
}
#endif

bool StringFormatterFor<StringView>::format(StringFormatOutput& data, const StringView specifier,
                                            const StringView value)
{
    SC_COMPILER_UNUSED(specifier);
    return data.append(value);
}

bool StringFormatterFor<String>::format(StringFormatOutput& data, const StringView specifier, const String& value)
{
    return StringFormatterFor<StringView>::format(data, specifier, value.view());
}

//-----------------------------------------------------------------------------------------------------------------------
// StringFormatOutput
//-----------------------------------------------------------------------------------------------------------------------
bool StringFormatOutput::append(StringView text)
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
            return data->append(text.toCharSpan());
        }
        else
        {
            return StringConverter::convertEncodingTo(encoding, text, *data, nullptr,
                                                      StringConverter::DoNotAddZeroTerminator);
        }
    }
    else
    {
        SC_ASSERT_DEBUG("StringFormatOutput::write - Forgot to set buffer or console" && 0);
        return false;
    }
}

StringFormatOutput::StringFormatOutput(StringEncoding encoding, Vector<char>& destination) : encoding(encoding)
{
    data    = &destination;
    console = nullptr;
}

StringFormatOutput::StringFormatOutput(StringEncoding encoding, Console& newConsole) : encoding(encoding)
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

bool StringFormatOutput::onFormatSucceeded()
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
        SC_ASSERT_RELEASE(data->resize(backupSize));
    }
}

} // namespace SC
