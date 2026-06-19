// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SerializationJson.h"
#include "../Common/CompilerBuiltins.h"
#include "../Common/Result.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace
{
using namespace SC;

bool appendControlEscape(SerializationTextOutput& output, uint32_t codePoint)
{
    constexpr char hex[] = "0123456789abcdef";
    switch (codePoint)
    {
    case '"': return output.append("\\\"");
    case '\\': return output.append("\\\\");
    case '\b': return output.append("\\b");
    case '\f': return output.append("\\f");
    case '\n': return output.append("\\n");
    case '\r': return output.append("\\r");
    case '\t': return output.append("\\t");
    default: break;
    }
    if (codePoint < 0x20)
    {
        char unicodeEscape[] = {'\\', 'u', '0', '0', hex[codePoint >> 4], hex[codePoint & 0x0f]};
        return output.append(StringSpan({unicodeEscape, sizeof(unicodeEscape)}, false, StringEncoding::Ascii));
    }
    return false;
}

bool appendUTF8CodePoint(SerializationTextOutput& output, uint32_t codePoint)
{
    char   bytes[4];
    size_t length = 0;
    if (codePoint <= 0x7f)
    {
        bytes[length++] = static_cast<char>(codePoint);
    }
    else if (codePoint <= 0x7ff)
    {
        bytes[length++] = static_cast<char>(0xc0 | (codePoint >> 6));
        bytes[length++] = static_cast<char>(0x80 | (codePoint & 0x3f));
    }
    else if (codePoint >= 0xd800 and codePoint <= 0xdfff)
    {
        return false;
    }
    else if (codePoint <= 0xffff)
    {
        bytes[length++] = static_cast<char>(0xe0 | (codePoint >> 12));
        bytes[length++] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
        bytes[length++] = static_cast<char>(0x80 | (codePoint & 0x3f));
    }
    else if (codePoint <= 0x10ffff)
    {
        bytes[length++] = static_cast<char>(0xf0 | (codePoint >> 18));
        bytes[length++] = static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f));
        bytes[length++] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
        bytes[length++] = static_cast<char>(0x80 | (codePoint & 0x3f));
    }
    else
    {
        return false;
    }
    return output.append(StringSpan({bytes, length}, false, StringEncoding::Utf8));
}

bool appendJSONStringEscapedUTF16(SerializationTextOutput& output, StringSpan text)
{
    const char* it  = text.bytesWithoutTerminator();
    const char* end = it + text.sizeInBytes();
    while (it < end)
    {
        const char* before    = it;
        uint32_t    codePoint = StringSpan::advanceUTF16(it, end);
        if (codePoint == 0)
        {
            uint16_t lead = 0;
            if (before + sizeof(uint16_t) > end)
            {
                return false;
            }
            CompilerBuiltins::copy(reinterpret_cast<char*>(&lead), before, sizeof(uint16_t));
            if (lead != 0)
            {
                return false;
            }
        }
        if (codePoint == '"' or codePoint == '\\' or codePoint < 0x20)
        {
            SC_TRY(appendControlEscape(output, codePoint));
        }
        else
        {
            SC_TRY(appendUTF8CodePoint(output, codePoint));
        }
    }
    return true;
}

bool appendJSONStringEscaped(SerializationTextOutput& output, StringSpan text)
{
    SC_TRY(output.append("\""));
    if (text.getEncoding() == StringEncoding::Utf16)
    {
        SC_TRY(appendJSONStringEscapedUTF16(output, text));
        return output.append("\"");
    }

    const char*  bytes     = text.bytesWithoutTerminator();
    const size_t length    = text.sizeInBytes();
    size_t       runFrom   = 0;
    auto         appendRun = [&](size_t until) -> bool
    {
        if (until <= runFrom)
        {
            return true;
        }
        return output.append(StringSpan({bytes + runFrom, until - runFrom}, false, text.getEncoding()));
    };

    for (size_t idx = 0; idx < length; ++idx)
    {
        const unsigned char current = static_cast<unsigned char>(bytes[idx]);
        if (current == '"' or current == '\\' or current < 0x20)
        {
            SC_TRY(appendRun(idx));
            SC_TRY(appendControlEscape(output, current));
            runFrom = idx + 1;
        }
    }
    SC_TRY(appendRun(length));
    return output.append("\"");
}

int hexValue(char value)
{
    if (value >= '0' and value <= '9')
    {
        return value - '0';
    }
    if (value >= 'a' and value <= 'f')
    {
        return 10 + value - 'a';
    }
    if (value >= 'A' and value <= 'F')
    {
        return 10 + value - 'A';
    }
    return -1;
}

bool containsJSONEscape(StringSpan escaped)
{
    const char*  bytes  = escaped.bytesWithoutTerminator();
    const size_t length = escaped.sizeInBytes();
    for (size_t idx = 0; idx < length; ++idx)
    {
        if (bytes[idx] == '\\')
        {
            return true;
        }
    }
    return false;
}

template <size_t N>
bool copyTokenToBuffer(StringSpan token, char (&buffer)[N])
{
    if (token.getEncoding() == StringEncoding::Utf16 or token.sizeInBytes() >= N)
    {
        return false;
    }
    CompilerBuiltins::copy(buffer, token.bytesWithoutTerminator(), token.sizeInBytes());
    buffer[token.sizeInBytes()] = 0;
    return true;
}

bool parseInt32(StringSpan token, int32_t& value)
{
    char buffer[32];
    SC_TRY(copyTokenToBuffer(token, buffer));
    errno        = 0;
    char* end    = nullptr;
    long  parsed = ::strtol(buffer, &end, 10);
    if (errno == 0 and buffer < end and parsed >= INT32_MIN and parsed <= INT32_MAX)
    {
        value = static_cast<int32_t>(parsed);
        return true;
    }
    return false;
}

bool parseFloat(StringSpan token, float& value)
{
    char buffer[64];
    SC_TRY(copyTokenToBuffer(token, buffer));
    char* end = nullptr;
    value     = ::strtof(buffer, &end);
    return buffer < end;
}

bool appendFormatted(SerializationTextOutput& output, const char* format, int value)
{
    char      buffer[32];
    const int numChars = ::snprintf(buffer, sizeof(buffer), format, value);
    if (numChars < 0 or static_cast<size_t>(numChars) >= sizeof(buffer))
    {
        return false;
    }
    return output.append(StringSpan({buffer, static_cast<size_t>(numChars)}, false, StringEncoding::Ascii));
}

bool appendFormatted(SerializationTextOutput& output, uint8_t floatDigits, double value)
{
    char      buffer[64];
    const int numChars = ::snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(floatDigits), value);
    if (numChars < 0 or static_cast<size_t>(numChars) >= sizeof(buffer))
    {
        return false;
    }
    return output.append(StringSpan({buffer, static_cast<size_t>(numChars)}, false, StringEncoding::Ascii));
}

} // namespace

bool SC::SerializationJson::Reader::appendJSONStringUnescaped(StringSpan escaped, SerializationTextOutput& output)
{
    const char*  bytes     = escaped.bytesWithoutTerminator();
    const size_t length    = escaped.sizeInBytes();
    size_t       runFrom   = 0;
    auto         appendRun = [&](size_t until) -> bool
    {
        if (until <= runFrom)
        {
            return true;
        }
        return output.append(StringSpan({bytes + runFrom, until - runFrom}, false, escaped.getEncoding()));
    };

    for (size_t idx = 0; idx < length; ++idx)
    {
        if (bytes[idx] != '\\')
        {
            continue;
        }
        SC_TRY(appendRun(idx));
        SC_TRY(idx + 1 < length);
        const char escape = bytes[++idx];
        switch (escape)
        {
        case '"': SC_TRY(output.append("\"")); break;
        case '\\': SC_TRY(output.append("\\")); break;
        case '/': SC_TRY(output.append("/")); break;
        case 'b': SC_TRY(output.append(StringSpan({"\b", 1}, false, StringEncoding::Ascii))); break;
        case 'f': SC_TRY(output.append(StringSpan({"\f", 1}, false, StringEncoding::Ascii))); break;
        case 'n': SC_TRY(output.append("\n")); break;
        case 'r': SC_TRY(output.append("\r")); break;
        case 't': SC_TRY(output.append("\t")); break;
        case 'u': {
            SC_TRY(idx + 4 < length);
            uint32_t codePoint = 0;
            for (size_t hexIndex = 0; hexIndex < 4; ++hexIndex)
            {
                const int value = hexValue(bytes[idx + 1 + hexIndex]);
                SC_TRY(value >= 0);
                codePoint = (codePoint << 4) | static_cast<uint32_t>(value);
            }
            idx += 4;
            SC_TRY(appendUTF8CodePoint(output, codePoint));
        }
        break;
        default: return false;
        }
        runFrom = idx + 1;
    }
    SC_TRY(appendRun(length));
    return true;
}

bool SC::SerializationJson::Writer::onSerializationStart()
{
    output.onFormatBegin();
    return setOptions(options);
}

bool SC::SerializationJson::Writer::onSerializationEnd() { return output.onFormatSucceeded(); }

bool SC::SerializationJson::Writer::setOptions(Options opt)
{
    options = opt;
    return true;
}

bool SC::SerializationJson::Writer::startObject(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("{");
}

bool SC::SerializationJson::Writer::endObject() { return output.append("}"); }

bool SC::SerializationJson::Writer::startArray(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("[");
}

bool SC::SerializationJson::Writer::endArray() { return output.append("]"); }

bool SC::SerializationJson::Writer::startObjectField(uint32_t index, StringSpan text)
{
    SC_TRY(eventuallyAddComma(index));
    return appendJSONStringEscaped(output, text) and output.append(":");
}

bool SC::SerializationJson::Writer::serializeStringSpan(uint32_t index, StringSpan text)
{
    SC_TRY(eventuallyAddComma(index));
    return appendJSONStringEscaped(output, text);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, StringSpan text)
{
    return serializeStringSpan(index, text);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, float value)
{
    SC_TRY(eventuallyAddComma(index));
    return appendFormatted(output, options.floatDigits, static_cast<double>(value));
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, double value)
{
    SC_TRY(eventuallyAddComma(index));
    return appendFormatted(output, options.floatDigits, value);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, int value)
{
    SC_TRY(eventuallyAddComma(index));
    return appendFormatted(output, "%d", value);
}

bool SC::SerializationJson::Writer::eventuallyAddComma(uint32_t index) { return index > 0 ? output.append(",") : true; }

bool SC::SerializationJson::Reader::startObject(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::ObjectStart);
    return true;
}

bool SC::SerializationJson::Reader::endObject()
{
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::ObjectEnd;
}

bool SC::SerializationJson::Reader::startArray(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::ArrayStart;
}

bool SC::SerializationJson::Reader::endArray()
{
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::ArrayEnd;
}

bool SC::SerializationJson::Reader::eventuallyExpectComma(uint32_t index)
{
    if (index > 0)
    {
        JsonTokenizer::Token token;
        SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Comma);
    }
    return true;
}

SC::StringSpan SC::SerializationJson::Reader::serializeInternal(uint32_t index, bool& succeeded)
{
    succeeded = false;
    if (eventuallyExpectComma(index))
    {
        JsonTokenizer::Token token;
        if (JsonTokenizer::tokenizeNext(iterator, token) and token.getType() == JsonTokenizer::Token::String)
        {
            succeeded = true;
            return token.getToken(iteratorText);
        }
    }
    return {};
}

bool SC::SerializationJson::Reader::serialize(uint32_t index, StringSpan& value)
{
    bool             succeeded;
    const StringSpan escaped = serializeInternal(index, succeeded);
    if (not succeeded or containsJSONEscape(escaped))
    {
        return false;
    }
    value = escaped;
    return true;
}

bool SC::SerializationJson::Reader::startObjectField(uint32_t index, StringSpan text)
{
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::String);
    SC_TRY(text == token.getToken(iteratorText));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::Colon;
}
bool SC::SerializationJson::Reader::getNextField(uint32_t index, StringSpan& text, bool& hasMore)
{
    auto                 iteratorBackup = iterator;
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    if (token.getType() == JsonTokenizer::Token::ObjectEnd)
    {
        iterator = iteratorBackup;
        hasMore  = false;
        return true;
    }
    iterator = iteratorBackup;
    SC_TRY(eventuallyExpectComma(index));
    hasMore = true;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::String);
    text = token.getToken(iteratorText);
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::Colon;
}

bool SC::SerializationJson::Reader::serialize(uint32_t index, bool& value)
{
    (void)(value);
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    if (token.getType() == JsonTokenizer::Token::True)
    {
        value = true;
        return true;
    }
    if (token.getType() == JsonTokenizer::Token::False)
    {
        value = false;
        return true;
    }
    return false;
}

bool SC::SerializationJson::Reader::serialize(uint32_t index, float& value)
{
    (void)(value);
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::Number);
    return parseFloat(token.getToken(iteratorText), value);
}

bool SC::SerializationJson::Reader::serialize(uint32_t index, int32_t& value)
{
    (void)(value);
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::Number);
    return parseInt32(token.getToken(iteratorText), value);
}

bool SC::SerializationJson::Reader::tokenizeArrayStart(uint32_t index)
{
    if (not eventuallyExpectComma(index))
        return false;
    JsonTokenizer::Token token;
    if (not JsonTokenizer::tokenizeNext(iterator, token))
        return false;
    if (token.getType() != JsonTokenizer::Token::ArrayStart)
        return false;
    return true;
}

bool SC::SerializationJson::Reader::tokenizeArrayEnd(uint32_t& size)
{
    auto                 iteratorBackup = iterator;
    JsonTokenizer::Token token;
    if (not JsonTokenizer::tokenizeNext(iterator, token))
        return false;
    iterator = iteratorBackup;
    if (token.getType() != JsonTokenizer::Token::ArrayEnd)
    {
        size += 1;
    }
    return true;
}
