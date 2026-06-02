// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SerializationJson.h"
#include "../Foundation/Result.h"
#include "Internal/JsonTokenizer.h"

#include <stdio.h>

namespace
{
using namespace SC;

bool appendJSONStringEscaped(StringFormatOutput& output, StringView text)
{
    SC_TRY(output.append("\""_a8));
    const char*  bytes     = text.bytesWithoutTerminator();
    const size_t length    = text.sizeInBytes();
    size_t       runFrom   = 0;
    auto         appendRun = [&](size_t until) -> bool
    {
        if (until <= runFrom)
        {
            return true;
        }
        return output.append(StringView({bytes + runFrom, until - runFrom}, false, text.getEncoding()));
    };

    constexpr char hex[] = "0123456789abcdef";
    for (size_t idx = 0; idx < length; ++idx)
    {
        const unsigned char current = static_cast<unsigned char>(bytes[idx]);
        StringView          escaped;
        switch (current)
        {
        case '"': escaped = "\\\""_a8; break;
        case '\\': escaped = "\\\\"_a8; break;
        case '\b': escaped = "\\b"_a8; break;
        case '\f': escaped = "\\f"_a8; break;
        case '\n': escaped = "\\n"_a8; break;
        case '\r': escaped = "\\r"_a8; break;
        case '\t': escaped = "\\t"_a8; break;
        default: break;
        }

        if (not escaped.isEmpty())
        {
            SC_TRY(appendRun(idx));
            SC_TRY(output.append(escaped));
            runFrom = idx + 1;
        }
        else if (current < 0x20)
        {
            char unicodeEscape[] = {'\\', 'u', '0', '0', hex[current >> 4], hex[current & 0x0f]};
            SC_TRY(appendRun(idx));
            SC_TRY(output.append(StringView({unicodeEscape, sizeof(unicodeEscape)}, false, StringEncoding::Ascii)));
            runFrom = idx + 1;
        }
    }
    SC_TRY(appendRun(length));
    return output.append("\""_a8);
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

bool containsJSONEscape(StringView escaped)
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

bool appendUTF8CodePoint(StringFormatOutput& output, uint32_t codePoint)
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
    return output.append(StringView({bytes, length}, false, StringEncoding::Utf8));
}

} // namespace

bool SC::SerializationJson::Reader::appendJSONStringUnescaped(StringView escaped, StringFormatOutput& output)
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
        return output.append(StringView({bytes + runFrom, until - runFrom}, false, escaped.getEncoding()));
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
        case '"': SC_TRY(output.append("\""_a8)); break;
        case '\\': SC_TRY(output.append("\\"_a8)); break;
        case '/': SC_TRY(output.append("/"_a8)); break;
        case 'b': SC_TRY(output.append(StringView({"\b", 1}, false, StringEncoding::Ascii))); break;
        case 'f': SC_TRY(output.append(StringView({"\f", 1}, false, StringEncoding::Ascii))); break;
        case 'n': SC_TRY(output.append("\n"_a8)); break;
        case 'r': SC_TRY(output.append("\r"_a8)); break;
        case 't': SC_TRY(output.append("\t"_a8)); break;
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
    ::snprintf(floatFormatStorage, sizeof(floatFormatStorage), ".%d", options.floatDigits);
    floatFormat = StringSpan::fromNullTerminated(floatFormatStorage, StringEncoding::Ascii);
    return true;
}

bool SC::SerializationJson::Writer::startObject(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("{"_a8);
}

bool SC::SerializationJson::Writer::endObject() { return output.append("}"_a8); }

bool SC::SerializationJson::Writer::startArray(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("["_a8);
}

bool SC::SerializationJson::Writer::endArray() { return output.append("]"_a8); }

bool SC::SerializationJson::Writer::startObjectField(uint32_t index, StringView text)
{
    SC_TRY(eventuallyAddComma(index));
    return appendJSONStringEscaped(output, text) and output.append(":"_a8);
}

bool SC::SerializationJson::Writer::serializeStringView(uint32_t index, StringView text)
{
    SC_TRY(eventuallyAddComma(index));
    return appendJSONStringEscaped(output, text);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, StringSpan text)
{
    return serializeStringView(index, text);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, StringView text)
{
    return serializeStringView(index, text);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, float value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<float>::format(output, floatFormat, value);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, double value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<double>::format(output, floatFormat, value);
}

bool SC::SerializationJson::Writer::serialize(uint32_t index, int value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<int>::format(output, StringView(), value);
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

SC::StringView SC::SerializationJson::Reader::serializeInternal(uint32_t index, bool& succeeded)
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
    const StringView escaped = serializeInternal(index, succeeded);
    if (not succeeded or containsJSONEscape(escaped))
    {
        return false;
    }
    value = escaped;
    return true;
}

bool SC::SerializationJson::Reader::serialize(uint32_t index, StringView& value)
{
    StringSpan span;
    if (not serialize(index, span))
    {
        return false;
    }
    value = span;
    return true;
}

bool SC::SerializationJson::Reader::startObjectField(uint32_t index, StringView text)
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
    return token.getToken(iteratorText).parseFloat(value);
}

bool SC::SerializationJson::Reader::serialize(uint32_t index, int32_t& value)
{
    (void)(value);
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseInt32(value);
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
