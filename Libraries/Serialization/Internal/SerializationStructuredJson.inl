// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SerializationStructuredJson.h"

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
    SC_TRY(eventuallyAddComma(index));
    return output.write("{"_a8);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::endObject() { return output.write("}"_a8); }

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::startArray(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.write("["_a8);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::endArray() { return output.write("]"_a8); }

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::startObjectField(uint32_t index, StringView text)
{
    SC_TRY(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.write("\"") and output.write(text) and output.write("\"") and output.write(":"_a8);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::serialize(uint32_t index, const String& value)
{
    SC_TRY(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.write("\"") and output.write(value.view()) and output.write("\"");
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::serialize(uint32_t index, float value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<float>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::serialize(uint32_t index, double value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<double>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructuredTemplate::SerializationJsonWriter::eventuallyAddComma(uint32_t index)
{
    return index > 0 ? output.write(",") : true;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::startObject(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.type == JsonTokenizer::Token::ObjectStart);
    return true;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::endObject()
{
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::ObjectEnd;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::startArray(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::ArrayStart;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::endArray()
{
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::ArrayEnd;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::eventuallyExpectComma(uint32_t index)
{
    if (index > 0)
    {
        SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
        SC_TRY(token.type == JsonTokenizer::Token::Comma);
    }
    return true;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::serialize(uint32_t index, String& text)
{
    SC_COMPILER_UNUSED(text);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.type == JsonTokenizer::Token::String);
    // TODO: Escape JSON string
    return text.assign(token.getToken(iteratorText));
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::startObjectField(uint32_t index, StringView text)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.type == JsonTokenizer::Token::String);
    SC_TRY(text == token.getToken(iteratorText));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::Colon;
}
bool SC::SerializationStructuredTemplate::SerializationJsonReader::getNextField(uint32_t index, StringView& text,
                                                                                bool& hasMore)
{
    auto iteratorBackup = iterator;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    if (token.type == JsonTokenizer::Token::ObjectEnd)
    {
        iterator = iteratorBackup;
        hasMore  = false;
        return true;
    }
    iterator = iteratorBackup;
    SC_TRY(eventuallyExpectComma(index));
    hasMore = true;
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.type == JsonTokenizer::Token::String);
    text = token.getToken(iteratorText);
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    return token.type == JsonTokenizer::Token::Colon;
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::serialize(uint32_t index, float& value)
{
    SC_COMPILER_UNUSED(value);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.type == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseFloat(value);
}

bool SC::SerializationStructuredTemplate::SerializationJsonReader::serialize(uint32_t index, int32_t& value)
{
    SC_COMPILER_UNUSED(value);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(JsonTokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.type == JsonTokenizer::Token::Number);
    return token.getToken(iteratorText).parseInt32(value);
}
