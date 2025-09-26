// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SerializationJson.h"
#include "../Foundation/Result.h"
#include "Internal/JsonTokenizer.h"

#include <stdio.h>
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
    // TODO: Escape JSON string
    return output.append("\"") and output.append(text) and output.append("\"") and output.append(":"_a8);
}

bool SC::SerializationJson::Writer::serializeStringView(uint32_t index, StringView text)
{
    SC_TRY(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.append("\"") and output.append(text) and output.append("\"");
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
    // TODO: Escape JSON string
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
    SC_COMPILER_UNUSED(value);
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
    SC_COMPILER_UNUSED(value);
    SC_TRY(eventuallyExpectComma(index));
    JsonTokenizer::Token token;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseFloat(value);
}

bool SC::SerializationJson::Reader::serialize(uint32_t index, int32_t& value)
{
    SC_COMPILER_UNUSED(value);
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
