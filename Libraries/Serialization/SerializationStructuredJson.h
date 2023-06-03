// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/StringBuilder.h"
#include "../Json/JsonTokenizer.h"

namespace SC
{
namespace SerializationStructuredTemplate
{
struct SerializationJsonWriter;
struct SerializationJsonReader;
} // namespace SerializationStructuredTemplate
} // namespace SC

struct SC::SerializationStructuredTemplate::SerializationJsonWriter
{
    struct Options
    {
        uint8_t floatDigits = 2;
    };

    StringFormatOutput& output;

    SerializationJsonWriter(StringFormatOutput& output) : output(output) {}

    [[nodiscard]] bool onSerializationStart();

    [[nodiscard]] bool onSerializationEnd();
    [[nodiscard]] bool setOptions(Options opt);

    [[nodiscard]] bool startObject(uint32_t index);

    [[nodiscard]] bool endObject();
    [[nodiscard]] bool startArray(uint32_t index);

    template <typename Container>
    [[nodiscard]] bool startArray(uint32_t index, Container& container, uint32_t& size)
    {
        SC_TRY_IF(eventuallyAddComma(index));
        size = static_cast<uint32_t>(container.size());
        return output.write("["_a8);
    }

    [[nodiscard]] bool endArray();

    template <typename Container>
    [[nodiscard]] bool arrayItem(Container&, uint32_t&)
    {
        return true;
    }

    [[nodiscard]] bool objectFieldName(uint32_t index, StringView text);

    [[nodiscard]] bool serialize(uint32_t index, const String& value);

    [[nodiscard]] bool serialize(uint32_t index, float value);
    [[nodiscard]] bool serialize(uint32_t index, double value);

    template <typename T>
    [[nodiscard]] bool serialize(uint32_t index, T value)
    {
        SC_TRY_IF(eventuallyAddComma(index));
        return StringFormatterFor<T>::format(output, StringView(), value);
    }

  private:
    bool eventuallyAddComma(uint32_t index);

    SmallString<10> floatFormat;
    Options         options;
};

struct SC::SerializationStructuredTemplate::SerializationJsonReader
{
    StringIteratorASCII  iterator;
    StringView           iteratorText;
    JsonTokenizer::Token token = {};

    [[nodiscard]] bool onSerializationStart() { return true; }
    [[nodiscard]] bool onSerializationEnd() { return true; }
    [[nodiscard]] bool startObject(uint32_t index);

    [[nodiscard]] bool endObject();

    [[nodiscard]] bool startArray(uint32_t index);

    template <typename Container>
    [[nodiscard]] bool startArray(uint32_t index, Container& container, uint32_t& size)
    {
        SC_TRY_IF(eventuallyExpectComma(index));
        SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
        SC_TRY_IF(token.type == JsonTokenizer::Token::ArrayStart);
        return arrayItem(container, size);
    }

    template <typename Container>
    [[nodiscard]] bool arrayItem(Container& container, uint32_t& size)
    {
        auto iteratorBackup = iterator;
        SC_TRY_IF(JsonTokenizer::tokenizeNext(iterator, token));
        if (token.type != JsonTokenizer::Token::ArrayEnd)
        {
            size += 1;
            SC_TRY_IF(container.resize(size));
        }
        iterator = iteratorBackup;
        return true;
    }

    [[nodiscard]] bool endArray();

    [[nodiscard]] bool eventuallyExpectComma(uint32_t index);

    [[nodiscard]] bool serialize(uint32_t index, String& text);

    [[nodiscard]] bool objectFieldName(uint32_t index, StringView text);
    [[nodiscard]] bool serialize(uint32_t index, float& value);

    [[nodiscard]] bool serialize(uint32_t index, int32_t& value);
};
