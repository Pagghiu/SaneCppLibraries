// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Json/JsonTokenizer.h"
#include "../Strings/SmallString.h"
#include "../Strings/StringBuilder.h"
namespace SC
{
namespace SerializationStructured
{
struct JsonWriter;
struct JsonReader;
} // namespace SerializationStructured
} // namespace SC

//! @addtogroup group_serialization_structured
//! @{

/// @brief Writer interface for Serializer that generates output JSON from C++ types.
/// Its methods are meant to be called by Serializer
struct SC::SerializationStructured::JsonWriter
{
    /// @brief Formatting options
    struct Options
    {
        uint8_t floatDigits = 2; ///< How many digits should be used when printing floating points
    };

    StringFormatOutput& output;

    JsonWriter(StringFormatOutput& output) : output(output) {}

    [[nodiscard]] bool onSerializationStart();
    [[nodiscard]] bool onSerializationEnd();

    [[nodiscard]] bool setOptions(Options opt);

    [[nodiscard]] bool startObject(uint32_t index);
    [[nodiscard]] bool endObject();

    [[nodiscard]] bool startArray(uint32_t index);
    [[nodiscard]] bool endArray();

    template <typename Container>
    [[nodiscard]] bool startArray(uint32_t index, Container& container, uint32_t& size)
    {
        if (not eventuallyAddComma(index))
            return false;
        size = static_cast<uint32_t>(container.size());
        return output.append("["_a8);
    }

    template <typename Container>
    [[nodiscard]] bool endArrayItem(Container&, uint32_t&)
    {
        return true;
    }

    [[nodiscard]] bool startObjectField(uint32_t index, StringView text);

    [[nodiscard]] bool serialize(uint32_t index, const String& value);
    [[nodiscard]] bool serialize(uint32_t index, float value);
    [[nodiscard]] bool serialize(uint32_t index, double value);

    template <typename T>
    [[nodiscard]] bool serialize(uint32_t index, T value)
    {
        if (not eventuallyAddComma(index))
            return false;
        return StringFormatterFor<T>::format(output, StringView(), value);
    }

  private:
    bool eventuallyAddComma(uint32_t index);

    SmallString<10> floatFormat;
    Options         options;
};

/// @brief Writer interface for Serializer that parses JSON into C++ types.
/// Its methods are meant to be called by Serializer
struct SC::SerializationStructured::JsonReader
{
    JsonReader(StringView text) : iteratorText(text), iterator(text.getIterator<StringIteratorASCII>()) {}

    [[nodiscard]] bool onSerializationStart() { return true; }
    [[nodiscard]] bool onSerializationEnd() { return true; }

    [[nodiscard]] bool startObject(uint32_t index);
    [[nodiscard]] bool endObject();

    [[nodiscard]] bool startArray(uint32_t index);
    [[nodiscard]] bool endArray();

    template <typename Container>
    [[nodiscard]] bool startArray(uint32_t index, Container& container, uint32_t& size)
    {
        if (not eventuallyExpectComma(index))
            return false;
        if (not JsonTokenizer::tokenizeNext(iterator, token))
            return false;
        if (token.getType() != JsonTokenizer::Token::ArrayStart)
            return false;
        return endArrayItem(container, size);
    }

    template <typename Container>
    [[nodiscard]] bool endArrayItem(Container& container, uint32_t& size)
    {
        auto iteratorBackup = iterator;
        if (not JsonTokenizer::tokenizeNext(iterator, token))
            return false;
        if (token.getType() != JsonTokenizer::Token::ArrayEnd)
        {
            size += 1;
            if (not container.resize(size))
                return false;
        }
        iterator = iteratorBackup;
        return true;
    }

    [[nodiscard]] bool startObjectField(uint32_t index, StringView text);
    [[nodiscard]] bool getNextField(uint32_t index, StringView& text, bool& hasMore);

    [[nodiscard]] bool serialize(uint32_t index, float& value);
    [[nodiscard]] bool serialize(uint32_t index, int32_t& value);
    [[nodiscard]] bool serialize(uint32_t index, String& text);

  private:
    [[nodiscard]] bool eventuallyExpectComma(uint32_t index);

    StringView           iteratorText;
    StringIteratorASCII  iterator;
    JsonTokenizer::Token token;
};

//! @}
