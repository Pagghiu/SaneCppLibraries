// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "SerializationStructuredJson.h"
#include "../Foundation/Result.h"

bool SC::SerializationStructured::JsonWriter::onSerializationStart()
{
    output.onFormatBegin();
    return setOptions(options);
}

bool SC::SerializationStructured::JsonWriter::onSerializationEnd() { return output.onFormatSucceded(); }

bool SC::SerializationStructured::JsonWriter::setOptions(Options opt)
{
    options = opt;
    StringBuilder builder(floatFormat);
    return builder.format(".{}", options.floatDigits);
}

bool SC::SerializationStructured::JsonWriter::startObject(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("{"_a8);
}

bool SC::SerializationStructured::JsonWriter::endObject() { return output.append("}"_a8); }

bool SC::SerializationStructured::JsonWriter::startArray(uint32_t index)
{
    SC_TRY(eventuallyAddComma(index));
    return output.append("["_a8);
}

bool SC::SerializationStructured::JsonWriter::endArray() { return output.append("]"_a8); }

bool SC::SerializationStructured::JsonWriter::startObjectField(uint32_t index, StringView text)
{
    SC_TRY(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.append("\"") and output.append(text) and output.append("\"") and output.append(":"_a8);
}

bool SC::SerializationStructured::JsonWriter::serialize(uint32_t index, const String& value)
{
    SC_TRY(eventuallyAddComma(index));
    // TODO: Escape JSON string
    return output.append("\"") and output.append(value.view()) and output.append("\"");
}

bool SC::SerializationStructured::JsonWriter::serialize(uint32_t index, float value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<float>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructured::JsonWriter::serialize(uint32_t index, double value)
{
    SC_TRY(eventuallyAddComma(index));
    return StringFormatterFor<double>::format(output, floatFormat.view(), value);
}

bool SC::SerializationStructured::JsonWriter::eventuallyAddComma(uint32_t index)
{
    return index > 0 ? output.append(",") : true;
}

bool SC::SerializationStructured::JsonReader::startObject(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == Json::Tokenizer::Token::ObjectStart);
    return true;
}

bool SC::SerializationStructured::JsonReader::endObject()
{
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    return token.getType() == Json::Tokenizer::Token::ObjectEnd;
}

bool SC::SerializationStructured::JsonReader::startArray(uint32_t index)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    return token.getType() == Json::Tokenizer::Token::ArrayStart;
}

bool SC::SerializationStructured::JsonReader::endArray()
{
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    return token.getType() == Json::Tokenizer::Token::ArrayEnd;
}

bool SC::SerializationStructured::JsonReader::eventuallyExpectComma(uint32_t index)
{
    if (index > 0)
    {
        SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Comma);
    }
    return true;
}

bool SC::SerializationStructured::JsonReader::serialize(uint32_t index, String& text)
{
    SC_COMPILER_UNUSED(text);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == Json::Tokenizer::Token::String);
    // TODO: Escape JSON string
    return text.assign(token.getToken(iteratorText));
}

bool SC::SerializationStructured::JsonReader::startObjectField(uint32_t index, StringView text)
{
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == Json::Tokenizer::Token::String);
    SC_TRY(text == token.getToken(iteratorText));
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    return token.getType() == Json::Tokenizer::Token::Colon;
}
bool SC::SerializationStructured::JsonReader::getNextField(uint32_t index, StringView& text, bool& hasMore)
{
    auto iteratorBackup = iterator;
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    if (token.getType() == Json::Tokenizer::Token::ObjectEnd)
    {
        iterator = iteratorBackup;
        hasMore  = false;
        return true;
    }
    iterator = iteratorBackup;
    SC_TRY(eventuallyExpectComma(index));
    hasMore = true;
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == Json::Tokenizer::Token::String);
    text = token.getToken(iteratorText);
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    return token.getType() == Json::Tokenizer::Token::Colon;
}

bool SC::SerializationStructured::JsonReader::serialize(uint32_t index, float& value)
{
    SC_COMPILER_UNUSED(value);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == Json::Tokenizer::Token::Number);
    return token.getToken(iteratorText).parseFloat(value);
}

bool SC::SerializationStructured::JsonReader::serialize(uint32_t index, int32_t& value)
{
    SC_COMPILER_UNUSED(value);
    SC_TRY(eventuallyExpectComma(index));
    SC_TRY(Json::Tokenizer::tokenizeNext(iterator, token));
    SC_TRY(token.getType() == Json::Tokenizer::Token::Number);
    return token.getToken(iteratorText).parseInt32(value);
}
