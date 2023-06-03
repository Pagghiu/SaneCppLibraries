// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "SerializationStructuredJson.h"

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::onSerializationStart()
{
    output.onFormatBegin();
    return setOptions(options);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::onSerializationEnd()
{
    return output.onFormatSucceded();
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::setOptions(Options opt)
{
    options = opt;
    StringBuilder builder(floatFormat);
    return builder.format(".{}", options.floatDigits);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::startObject(uint32_t index)
{
    SC_TRY_IF(eventuallyAddComma(index));
    return output.write("{"_a8);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::endObject() { return output.write("}"_a8); }

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::startArray(uint32_t index)
{
    SC_TRY_IF(eventuallyAddComma(index));
    return output.write("["_a8);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::endArray() { return output.write("]"_a8); }

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::objectFieldName(uint32_t index, StringView text)
{
    SC_TRY_IF(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.write("\"") and output.write(text) and output.write("\"") and output.write(":"_a8);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::serialize(uint32_t index, const String& value)
{
    SC_TRY_IF(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.write("\"") and output.write(value.view()) and output.write("\"");
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::serialize(uint32_t index, float value)
{
    SC_TRY_IF(eventuallyAddComma(index));
    return StringFormatterFor<float>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::serialize(uint32_t index, double value)
{
    SC_TRY_IF(eventuallyAddComma(index));
    return StringFormatterFor<double>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::eventuallyAddComma(uint32_t index)
{
    return index > 0 ? output.write(",") : true;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::startObject(uint32_t index)
{
    SC_TRY_IF(eventuallyExpectComma(index));
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY_IF(token.type == JsonTokenizer::Token::ObjectStart);
    return true;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::endObject()
{
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::ObjectEnd;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::startArray(uint32_t index)
{
    SC_TRY_IF(eventuallyExpectComma(index));
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::ArrayStart;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::endArray()
{
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::ArrayEnd;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::eventuallyExpectComma(uint32_t index)
{
    if (index > 0)
    {
        SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
        SC_TRY_IF(token.type == JsonTokenizer::Token::Comma);
    }
    return true;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::serialize(uint32_t index, String& text)
{
    SC_UNUSED(text);
    SC_TRY_IF(eventuallyExpectComma(index));
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY_IF(token.type == JsonTokenizer::Token::String);
    // TODO: Escape JSON string
    return text.assign(token.getToken(iteratorText));
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::objectFieldName(uint32_t index, StringView text)
{
    SC_UNUSED(text);
    SC_TRY_IF(eventuallyExpectComma(index));
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY_IF(token.type == JsonTokenizer::Token::String);
    SC_TRY_IF(text == token.getToken(iteratorText));
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::Colon;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::serialize(uint32_t index, float& value)
{
    SC_UNUSED(value);
    SC_TRY_IF(eventuallyExpectComma(index));
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY_IF(token.type == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseFloat(value);
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::serialize(uint32_t index, int32_t& value)
{
    SC_UNUSED(value);
    SC_TRY_IF(eventuallyExpectComma(index));
    SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY_IF(token.type == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseInt32(value);
}
