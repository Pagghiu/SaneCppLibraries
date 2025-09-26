// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Strings/StringFormat.h" //StringFormatOutput
#include "Internal/SerializationTextReadVersioned.h"
#include "Internal/SerializationTextReadWriteExact.h"

namespace SC
{
struct SC_COMPILER_EXPORT SerializationJson;
} // namespace SC

//! @defgroup group_serialization_text Serialization Text
//! @copybrief library_serialization_text (see @ref library_serialization_text for more details)
///
/// Serialization Text uses [Reflection](@ref library_reflection), to serialize C++ objects to text based formats that
/// currently includes a JSON serializer / deserializer.

//! @addtogroup group_serialization_text
//! @{

/// @brief SC::SerializationJson reads or writes C++ structures to / from json using @ref library_reflection information
///
/// @n
/// Let's consider the following structure described by [Reflection](@ref library_reflection):
/// @snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonSnippet1
///
/// This is how you can serialize the class to JSON
/// @snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonWriteSnippet
///
/// This is how you can de-serialize the class from JSON, matching fields by their label name, even if they come in
/// different order than the original class or even if there are missing field.
/// @snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonLoadVersionedSnippet
///
/// This is a special loader to deserialize the class from the exact same JSON that was output by the serializer itself.
/// Whitespace changes are fine, but changing the order in which two fields exists or removing one will make
/// deserialization fail. If these limitations are fine for the usage (for example the generated json files are not
/// meant to be manually edited by users) than maybe it could be worth using it, as this code path is a lot simpler (*)
/// than SC::SerializationJson::loadVersioned.
/// @snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonLoadExactSnippet
/// @note (*) simpler code probably means faster code, even if it has not been properly benchmarked yet so the
/// hypothetical performance gain is yet to be defined.
struct SC::SerializationJson
{
    /// @brief Formatting options
    struct SC_COMPILER_EXPORT Options
    {
        uint8_t floatDigits; ///< How many digits should be used when printing floating points
        Options() { floatDigits = 2; }
    };

    /// @brief Writes a C++ object to JSON using Reflection.
    /// Uses the strings associated with fields in [Reflection](@ref library_reflection) to generate a JSON
    /// representation of a given C++ serializable structure.
    /// @tparam T Type of object to write
    /// @param object Object to write
    /// @param output Output string interface
    /// @param options JSON formatting options
    /// @return `true` if write succeeded
    /// @see SC::SerializationJson for example usage
    template <typename T>
    [[nodiscard]] static bool write(T& object, StringFormatOutput& output, Options options = Options())
    {
        Writer stream(output, options);
        if (not stream.onSerializationStart())
            return false;
        if (not Serialization::SerializationTextReadWriteExact<Writer, T>::serialize(0, object, stream))
            return false;
        return stream.onSerializationEnd();
    }

    /// @brief Parses a JSON produced by SerializationJson::write loading its values into a C++ object
    /// Read a JSON buffer as output by SerializationJson::write, that means mainly not changing the relative order
    /// of struct fields in the json text.
    /// @note This code path does less checks that SerializationJson::loadVersioned and it could be potentially faster.
    /// @tparam T Type of object to load
    /// @param object Object to load
    /// @param text Json text to be deserialized
    /// @return `true` if load succeeded
    /// @see SC::SerializationJson for example usage
    template <typename T>
    [[nodiscard]] static bool loadExact(T& object, StringView text)
    {
        Reader stream(text);
        return Serialization::SerializationTextReadWriteExact<Reader, T>::serialize(0, object, stream);
    }

    /// @brief Parses a JSON buffer and writes C++ objects supporting reordered or missing fields.
    /// @tparam T Type of object load
    /// @param object Object to load
    /// @param text Json text to be deserialized
    /// @return `true` if load succeeded
    /// @see SC::SerializationJson for example usage
    template <typename T>
    [[nodiscard]] static bool loadVersioned(T& object, StringView text)
    {
        Reader stream(text);
        return Serialization::SerializationTextReadVersioned<Reader, T>::loadVersioned(0, object, stream);
    }

  private:
    /// @brief Writer interface for Serializer that generates output JSON from C++ types.
    /// Its methods are meant to be called by Serializer
    struct SC_COMPILER_EXPORT Writer
    {
        StringFormatOutput& output;

        Writer(StringFormatOutput& output, Options options) : output(output), options(options) {}

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

        template <typename T>
        [[nodiscard]] bool serialize(uint32_t index, T& text)
        {
            return serializeStringView(index, text.view());
        }

        [[nodiscard]] bool serialize(uint32_t index, float value);
        [[nodiscard]] bool serialize(uint32_t index, double value);
        [[nodiscard]] bool serialize(uint32_t index, int value);

      private:
        [[nodiscard]] bool serializeStringView(uint32_t index, StringView text);

        bool eventuallyAddComma(uint32_t index);

        char       floatFormatStorage[5];
        StringSpan floatFormat;
        Options    options;
    };

    /// @brief Writer interface for Serializer that parses JSON into C++ types.
    /// Its methods are meant to be called by Serializer
    struct SC_COMPILER_EXPORT Reader
    {
        Reader(StringView text) : iteratorText(text), iterator(text.getIterator<StringIteratorASCII>()) {}

        [[nodiscard]] bool onSerializationStart() { return true; }
        [[nodiscard]] bool onSerializationEnd() { return true; }

        [[nodiscard]] bool startObject(uint32_t index);
        [[nodiscard]] bool endObject();

        [[nodiscard]] bool startArray(uint32_t index);
        [[nodiscard]] bool endArray();

        template <typename Container>
        [[nodiscard]] bool startArray(uint32_t index, Container& container, uint32_t& size)
        {
            if (not tokenizeArrayStart(index))
                return false;
            return endArrayItem(container, size);
        }

        template <typename Container>
        [[nodiscard]] bool endArrayItem(Container& container, uint32_t& size)
        {
            auto oldSize = size;
            if (not tokenizeArrayEnd(size))
                return false;
            if (oldSize != size)
                return Reflection::ExtendedTypeInfo<Container>::resize(container, size);
            return true;
        }

        [[nodiscard]] bool startObjectField(uint32_t index, StringView text);
        [[nodiscard]] bool getNextField(uint32_t index, StringSpan& text, bool& hasMore);

        [[nodiscard]] bool serialize(uint32_t index, bool& value);
        [[nodiscard]] bool serialize(uint32_t index, float& value);
        [[nodiscard]] bool serialize(uint32_t index, int32_t& value);

        template <typename T>
        [[nodiscard]] bool serialize(uint32_t index, T& text)
        {
            bool succeeded;
            auto result = serializeInternal(index, succeeded);
            return succeeded and text.assign(result);
        }

      private:
        [[nodiscard]] StringView serializeInternal(uint32_t index, bool& succeeded);

        [[nodiscard]] bool tokenizeArrayStart(uint32_t index);
        [[nodiscard]] bool tokenizeArrayEnd(uint32_t& size);
        [[nodiscard]] bool eventuallyExpectComma(uint32_t index);

        StringView          iteratorText;
        StringIteratorASCII iterator;
    };
};
//! @}
