// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "SerializationStructuredJson.h"

bool SC::SerializationStructured::SerializationJsonWriter::onSerializationStart()
{
    output.onFormatBegin();
    return setOptions(options);
}

bool SC::SerializationStructured::SerializationJsonWriter::onSerializationEnd() { return output.onFormatSucceded(); }

bool SC::SerializationStructured::SerializationJsonWriter::setOptions(Options opt)
{
    options = opt;
    StringBuilder builder(floatFormat);
    return builder.format(".{}", options.floatDigits);
}

bool SC::SerializationStructured::SerializationJsonWriter::startObject(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("{"_a8);
}

bool SC::SerializationStructured::SerializationJsonWriter::endObject() { return output.append("}"_a8); }

bool SC::SerializationStructured::SerializationJsonWriter::startArray(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("["_a8);
}

bool SC::SerializationStructured::SerializationJsonWriter::endArray() { return output.append("]"_a8); }

bool SC::SerializationStructured::SerializationJsonWriter::startObjectField(uint32_t index, StringView text)
{
    SC_TRY(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.append("\"") and output.append(text) and output.append("\"") and output.append(":"_a8);
}

bool SC::SerializationStructured::SerializationJsonWriter::serialize(uint32_t index, const String& value)
{
    SC_TRY(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.append("\"") and output.append(value.view()) and output.append("\"");
}

bool SC::SerializationStructured::SerializationJsonWriter::serialize(uint32_t index, float value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<float>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructured::SerializationJsonWriter::serialize(uint32_t index, double value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<double>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructured::SerializationJsonWriter::eventuallyAddComma(uint32_t index)
{
    return index > 0 ? output.append(",") : true;
}

bool SC::SerializationStructured::SerializationJsonReader::startObject(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::ObjectStart);
    return true;
}

bool SC::SerializationStructured::SerializationJsonReader::endObject()
{
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::ObjectEnd;
}

bool SC::SerializationStructured::SerializationJsonReader::startArray(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::ArrayStart;
}

bool SC::SerializationStructured::SerializationJsonReader::endArray()
{
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::ArrayEnd;
}

bool SC::SerializationStructured::SerializationJsonReader::eventuallyExpectComma(uint32_t index)
{
    if (index > 0)
    {
        SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Comma);
    }
    return true;
}

bool SC::SerializationStructured::SerializationJsonReader::serialize(uint32_t index, String& text)
{
    SC_COMPILER_UNUSED(text);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::String);
    // TODO: Escape JSON string
    return text.assign(token.getToken(iteratorText));
}

bool SC::SerializationStructured::SerializationJsonReader::startObjectField(uint32_t index, StringView text)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::String);
    SC_TRY(text == token.getToken(iteratorText));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.getType() == JsonTokenizer::Token::Colon;
}
bool SC::SerializationStructured::SerializationJsonReader::getNextField(uint32_t index, StringView& text, bool& hasMore)
{
    auto iteratorBackup = iterator;
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

bool SC::SerializationStructured::SerializationJsonReader::serialize(uint32_t index, float& value)
{
    SC_COMPILER_UNUSED(value);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseFloat(value);
}

bool SC::SerializationStructured::SerializationJsonReader::serialize(uint32_t index, int32_t& value)
{
    SC_COMPILER_UNUSED(value);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseInt32(value);
}
