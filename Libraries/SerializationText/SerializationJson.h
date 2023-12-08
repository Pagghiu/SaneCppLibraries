// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Strings/SmallString.h"
#include "../Strings/StringFormat.h" //StringFormatOutput
#include "Internal/SerializationTextReadVersioned.h"
#include "Internal/SerializationTextReadWriteExact.h"

namespace SC
{
struct SerializationJson;
} // namespace SC

//! @defgroup group_serialization_text Serialization Text
//! @copybrief library_serialization_text (see @ref library_serialization_text for more details)

//! @addtogroup group_serialization_text
//! @{

/// @brief Reads or writes C++ structures to / from json using @ref library_reflection information
struct SC::SerializationJson
{
    /// @brief Formatting options
    struct Options
    {
        uint8_t floatDigits; ///< How many digits should be used when printing floating points
        Options() { floatDigits = 2; }
    };

    /// @brief Writes a C++ object to JSON using Reflection
    /// @tparam T Type of object to write
    /// @param object Object to write
    /// @param output Output string interface
    /// @param options JSON formatting options
    /// @return `true` if write succeeded
    template <typename T>
    [[nodiscard]] static bool write(T& object, StringFormatOutput& output, Options options = Options())
    {
        Writer stream(output, options);
        if (not stream.onSerializationStart())
            return false;
        if (not detail::SerializationTextReadWriteExact<Writer, T>::serialize(0, object, stream))
            return false;
        return stream.onSerializationEnd();
    }

    /// @brief Parses a JSON produced by SerializationJson::write loading its values into a C++ object
    /// @tparam T Type of object to load
    /// @param object Object to load
    /// @param text Json text to be deserialized
    /// @return `true` if load succeded
    template <typename T>
    [[nodiscard]] static bool loadExact(T& object, StringView text)
    {
        Reader stream(text);
        return detail::SerializationTextReadWriteExact<Reader, T>::serialize(0, object, stream);
    }

    /// @brief Parses a JSON loading its values into a C++ object
    /// @tparam T Type of object load
    /// @param object Object to load
    /// @param text Json text to be deserialized
    /// @return `true` if load succeded
    template <typename T>
    [[nodiscard]] static bool loadVersioned(T& object, StringView text)
    {
        Reader stream(text);
        return detail::SerializationTextReadVersioned<Reader, T>::loadVersioned(0, object, stream);
    }

  private:
    /// @brief Writer interface for Serializer that generates output JSON from C++ types.
    /// Its methods are meant to be called by Serializer
    struct Writer
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
    struct Reader
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
                return container.resize(size);
            return true;
        }

        [[nodiscard]] bool startObjectField(uint32_t index, StringView text);
        [[nodiscard]] bool getNextField(uint32_t index, StringView& text, bool& hasMore);

        [[nodiscard]] bool serialize(uint32_t index, float& value);
        [[nodiscard]] bool serialize(uint32_t index, int32_t& value);
        [[nodiscard]] bool serialize(uint32_t index, String& text);

      private:
        [[nodiscard]] bool tokenizeArrayStart(uint32_t index);
        [[nodiscard]] bool tokenizeArrayEnd(uint32_t& size);
        [[nodiscard]] bool eventuallyExpectComma(uint32_t index);

        StringView          iteratorText;
        StringIteratorASCII iterator;
    };
};
//! @}
