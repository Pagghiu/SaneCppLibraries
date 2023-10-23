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

// TODO: This class could be deleted potentially, unless we like to allow some dynamic formatting
struct SC::JsonFormatter
{
    enum State
    {
        Array,
        ArrayFirst,
        ObjectFirst,
        Object,
        ObjectValue
    };
    JsonFormatter(Vector<State>& state, StringFormatOutput& output);
    struct Options
    {
        uint8_t floatDigits = 2;
    };
    [[nodiscard]] bool setOptions(Options options);
    [[nodiscard]] bool writeFloat(float value);
    [[nodiscard]] bool writeDouble(double value);
    [[nodiscard]] bool writeInt8(int8_t value);
    [[nodiscard]] bool writeUint8(uint8_t value);
    [[nodiscard]] bool writeInt32(int32_t value);
    [[nodiscard]] bool writeUint32(uint32_t value);
    [[nodiscard]] bool writeInt64(int64_t value);
    [[nodiscard]] bool writeUint64(uint64_t value);
    [[nodiscard]] bool writeString(StringView value);
    [[nodiscard]] bool writeBoolean(bool value);
    [[nodiscard]] bool writeNull();
    [[nodiscard]] bool startArray();
    [[nodiscard]] bool endArray();
    [[nodiscard]] bool startObject();
    [[nodiscard]] bool startObjectField(StringView name);
    [[nodiscard]] bool endObject();

  private:
    Options options;
    char    floatFormat[10];
    bool    onBeforeValue();
    bool    onAfterValue();
    bool    writeSeparator();

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
