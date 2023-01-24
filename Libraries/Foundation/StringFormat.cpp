// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringFormat.h"

#include <inttypes.h> // PRIu64 / PRIi64
#include <stdio.h>    // snprintf

namespace SC
{
struct SpanContainerAdapter
{
    Span<char_t>       span;
    [[nodiscard]] bool insertCopy(size_t offsetInBytes, const char* source, size_t sourceSizeInBytes)
    {
        if (sourceSizeInBytes + offsetInBytes <= span.sizeInBytes())
        {
            memcpy(span.data() + offsetInBytes, source, sourceSizeInBytes * sizeof(char));
            return true;
        }
        return false;
    }
};

const int SPECIFIER_SIZE = 10;

template <size_t SPECIFIER_OFFSET, size_t N>
static bool transformToPrintfSpecifier(StringIteratorASCII specifier, char (&formatSpecifier)[N])
{
    Span<char_t>         span(formatSpecifier, N - SPECIFIER_OFFSET);
    SpanContainerAdapter adapter;
    adapter.span = span;
    SC_TRY_IF(specifier.insertBytesTo(adapter, SPECIFIER_OFFSET));
    return true;
}

template <size_t SPECIFIER_OFFSET = 1, size_t BUFFER_SIZE = 100, size_t N, typename Value>
bool formatSprintf(Vector<char_t>& data, char (&formatSpecifier)[N], StringIteratorASCII specifier, const Value value)
{
    if (not transformToPrintfSpecifier<SPECIFIER_OFFSET, N>(specifier, formatSpecifier))
        return false;
    char_t     buffer[BUFFER_SIZE];
    const int  numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    const bool validResult = numCharsExcludingTerminator >= 0 && numCharsExcludingTerminator + 1 < BUFFER_SIZE;
    return validResult && data.appendCopy(buffer, numCharsExcludingTerminator);
}
#if SC_MSVC
#else

bool StringFormatterFor<SC::size_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                            const SC::size_t value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%zu";
    return formatSprintf<ConstantArraySize("zu") - 1>(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::ssize_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                             const SC::ssize_t value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%zd";
    return formatSprintf<ConstantArraySize("zd") - 1>(data, formatSpecifier, specifier, value);
}
#endif

bool StringFormatterFor<SC::int64_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                             const SC::int64_t value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%" PRIi64;
    return formatSprintf<ConstantArraySize(PRIi64) - 1>(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint64_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                              const SC::uint64_t value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%" PRIu64;
    return formatSprintf<ConstantArraySize(PRIu64) - 1>(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::int32_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                             const SC::int32_t value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::uint32_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                              const SC::uint32_t value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%d";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::int16_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                             const SC::int16_t value)
{
    return StringFormatterFor<SC::int32_t>::format(data, specifier, value);
}

bool StringFormatterFor<SC::uint16_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                              const SC::uint16_t value)
{
    return StringFormatterFor<SC::uint32_t>::format(data, specifier, value);
}

bool StringFormatterFor<float>::format(Vector<char_t>& data, StringIteratorASCII specifier, const float value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<double>::format(Vector<char_t>& data, const StringIteratorASCII specifier, const double value)
{
    char_t formatSpecifier[SPECIFIER_SIZE] = "%f";
    return formatSprintf(data, formatSpecifier, specifier, value);
}

bool StringFormatterFor<SC::char_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                            const SC::char_t value)
{
    return data.appendCopy(&value, 1);
}

bool StringFormatterFor<const SC::char_t*>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                                   const SC::char_t* value)
{
    return data.appendCopy(value, strlen(value));
}

bool StringFormatterFor<SC::StringView>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                                const SC::StringView value)
{
    return data.appendCopy(value.bytesIncludingTerminator(), value.sizeInBytes());
}
} // namespace SC
