#include "StringFormat.h"

#include <inttypes.h> // PRIu64 / PRIi64
#include <stdarg.h>   // va_list
#include <stdio.h>    // snprintf

namespace SC
{
namespace text
{
template <size_t N, size_t OFFSET = 1>
static bool transformToPrintfSpecifier(StringIteratorASCII specifier, char (&formatSpecifier)[N])
{
    Span<char_t> span(formatSpecifier, N - OFFSET);
    SC_TRY_IF(specifier.insertBytesTo(span, OFFSET));
    return true;
}
#if 0 // this is not needed anymore :)
bool c_snprintf(Vector<char>& data, bool append, bool popzero, const char_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const size_t preserve = append ? data.isEmpty() ? 0 : popzero ? data.size() - 1 : data.size() : 0;
    size_t       bufLen   = data.capacity() - preserve;
    char_t*      buf      = data.begin() + preserve;
    int          newLen   = ::vsnprintf(buf, bufLen, fmt, args);
    if (newLen < 0)
    {
        va_end(args);
        return false;
    }
    const size_t newSizeWithZero = static_cast<size_t>(newLen + 1);
    if (newSizeWithZero > bufLen)
    {
        // Not enough space
        if (not data.resizeWithoutInitializing(preserve + newSizeWithZero))
        {
            va_end(args);
            return false;
        }
        buf    = data.begin() + preserve;
        bufLen = newSizeWithZero;
        newLen = ::vsnprintf(buf, bufLen, fmt, args);
        va_end(args);
        return newLen + 1 == newSizeWithZero;
    }
    va_end(args);
    return true;
}
#endif

bool StringFormatterFor<SC::size_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                            const SC::size_t value)
{
    char_t formatSpecifier[10] = "%zu";
    if (not transformToPrintfSpecifier<sizeof(formatSpecifier), 2>(specifier, formatSpecifier))
        return false;
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
}

bool StringFormatterFor<SC::ssize_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                             const SC::ssize_t value)
{
    char_t formatSpecifier[10] = "%zd";
    if (not transformToPrintfSpecifier<sizeof(formatSpecifier), 2>(specifier, formatSpecifier))
        return false;
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
}

bool StringFormatterFor<SC::int64_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                             const SC::int64_t value)
{
    char_t formatSpecifier[10] = "%" PRIi64;
    if (not transformToPrintfSpecifier<sizeof(formatSpecifier), ConstantStringLength(PRIu64)>(specifier,
                                                                                              formatSpecifier))
        return false;
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
}

bool StringFormatterFor<SC::uint64_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                              const SC::uint64_t value)
{
    char_t formatSpecifier[10] = "%" PRIu64;
    if (not transformToPrintfSpecifier<sizeof(formatSpecifier), ConstantStringLength(PRIu64)>(specifier,
                                                                                              formatSpecifier))
        return false;
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
}

bool StringFormatterFor<SC::int32_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                             const SC::int32_t value)
{
    char_t formatSpecifier[10] = "%d";
    SC_TRY_IF(transformToPrintfSpecifier(specifier, formatSpecifier));
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
}

bool StringFormatterFor<SC::uint32_t>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                              const SC::uint32_t value)
{
    char_t formatSpecifier[10] = "%d";
    SC_TRY_IF(transformToPrintfSpecifier(specifier, formatSpecifier));
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
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
    char_t formatSpecifier[10] = "%f";
    SC_TRY_IF(transformToPrintfSpecifier(specifier, formatSpecifier));
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
}

bool StringFormatterFor<double>::format(Vector<char_t>& data, const StringIteratorASCII specifier, const double value)
{
    char_t formatSpecifier[10] = "%f";
    SC_TRY_IF(transformToPrintfSpecifier(specifier, formatSpecifier));
    char_t    buffer[100];
    const int numCharsExcludingTerminator = snprintf(buffer, sizeof(buffer), formatSpecifier, value);
    return data.appendCopy(buffer, numCharsExcludingTerminator);
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
    return data.appendCopy(value.bytesIncludingTerminator(), value.sizeInBytesWithoutTerminator());
}
} // namespace text
} // namespace SC
