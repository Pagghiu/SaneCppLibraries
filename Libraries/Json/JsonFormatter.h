// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Strings/StringView.h"

namespace SC
{
struct JsonFormatter;

struct StringFormatOutput;
template <typename T>
struct Vector;
} // namespace SC

//! @defgroup group_json JSON
//! @copybrief library_json (see @ref library_json for more details)

//! @addtogroup group_json
//! @{

// TODO: This class could be deleted potentially, unless we like to allow some dynamic formatting

/// @brief Write correctly formatted JSON
struct SC::JsonFormatter
{
    /// @brief Possible states for the formatter state machine
    enum State
    {
        Array,       ///< Non first element of array
        ArrayFirst,  ///< First element of array
        Object,      ///< Non first element of object
        ObjectFirst, ///< First element of object
        ObjectValue  ///< Printing an object value
    };
    /// @brief Construct with a buffer for holding state machine states and a output format
    /// @param state Buffer holding state machine states
    /// @param output Output interface to print resulting json to
    JsonFormatter(Vector<State>& state, StringFormatOutput& output);

    /// @brief Formatting options (floating point digits)
    struct Options
    {
        uint8_t floatDigits = 2; ///< How many significant digits should be printed for floats
    };

    /// @brief Set options for formatting (floating point digits)
    /// @param options Options object with the wanted options
    /// @return `true` if options are valid
    [[nodiscard]] bool setOptions(Options options);

    /// @brief Write a json float value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeFloat(float value);

    /// @brief Write a json double value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeDouble(double value);

    /// @brief Write a json byte value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeInt8(int8_t value);

    /// @brief Write a json unsigned byte value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeUint8(uint8_t value);

    /// @brief Write a json 4 bytes integer value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeInt32(int32_t value);

    /// @brief Write a json 4 bytes unsigned integer value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeUint32(uint32_t value);

    /// @brief Write a json 8 bytes integer value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeInt64(int64_t value);

    /// @brief Write a json 8 bytes unsigned integer value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeUint64(uint64_t value);

    /// @brief Write a json String value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeString(StringView value);

    /// @brief Write a json boolean value
    /// @param value The value to write
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeBoolean(bool value);

    /// @brief Write a json null value
    /// @return `true` if the value has been written successfully
    [[nodiscard]] bool writeNull();

    /// @brief Starts an array
    /// @return `true` if the state machine allows starting an array now
    [[nodiscard]] bool startArray();

    /// @brief Ends an array
    /// @return `true` if the state machine allows ending an array now
    [[nodiscard]] bool endArray();

    /// @brief Starts an object
    /// @return `true` if the state machine allows starting an object here
    [[nodiscard]] bool startObject();

    /// @brief Starts an object field
    /// @param name The name of the object field
    /// @return `true` if the state machine allows starting an object field here
    [[nodiscard]] bool startObjectField(StringView name);

    /// @brief Ends an object
    /// @return `true` if the state machine allows ending an object here
    [[nodiscard]] bool endObject();

  private:
    Options options;
    char    floatFormat[10];

    bool onBeforeValue();
    bool onAfterValue();
    bool writeSeparator();

    enum RunState
    {
        BeforeStart,
        Running,
        AfterEnd
    };
    RunState runState = BeforeStart;

    Vector<State>&      state;
    StringFormatOutput& output;

    bool writeValue(int writtenChars, const char* buffer);
};

//! @}
