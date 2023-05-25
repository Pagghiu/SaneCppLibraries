// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Foundation/StringFormat.h"
#include "../Foundation/StringView.h"
#include "../Foundation/Vector.h"
#include "JsonStreamWriter.h"

#include <stdio.h> // snprintf

SC::JsonStreamWriter::JsonStreamWriter(Vector<State>& state, StringFormatOutput& output) : state(state), output(output)
{}

bool SC::JsonStreamWriter::writeSeparator()
{
    if (not state.isEmpty())
    {
        bool  printComma = true;
        auto& currState  = state.back();
        if (currState == ArrayFirst)
        {
            currState  = Array;
            printComma = false;
        }
        else if (currState == ObjectFirst)
        {
            currState  = Object;
            printComma = false;
        }
        else if (currState == ObjectValue)
        {
            printComma = false;
        }
        if (printComma)
        {
            return output.write(","_a8);
        }
    }
    return true;
}

bool SC::JsonStreamWriter::onBeforeValue()
{
    if (runState == BeforeStart)
    {
        output.onFormatBegin();
        runState = Running;
    }
    else
    {
        if (state.isEmpty())
        {
            return false;
        }
        auto currState = state.back();
        if (currState != ObjectValue and currState != Array and currState != ArrayFirst)
        {
            return false;
        }
    }
    return true;
}

bool SC::JsonStreamWriter::onAfterValue()
{
    if (state.isEmpty())
    {
        runState = AfterEnd;
        return output.onFormatSucceded();
    }
    if (state.back() == ObjectValue)
    {
        return state.pop_back();
    }
    return true;
}

bool SC::JsonStreamWriter::writeValue(int writtenChars, const char* buffer)
{
    if (not onBeforeValue() or not writeSeparator())
    {
        return false;
    }
    if (writtenChars > 0)
    {
        if (output.write(StringView(buffer, static_cast<size_t>(writtenChars), true, StringEncoding::Ascii)))
        {
            return onAfterValue();
        }
    }
    return false;
}

bool SC::JsonStreamWriter::writeFloat(float value)
{
    char buffer[100];
    return writeValue(snprintf(buffer, sizeof(buffer), "%f", value), buffer);
}

bool SC::JsonStreamWriter::writeDouble(double value)
{
    char buffer[100];
    return writeValue(snprintf(buffer, sizeof(buffer), "%f", value), buffer);
}

bool SC::JsonStreamWriter::writeInt8(int8_t value)
{
    char buffer[30];
    return writeValue(snprintf(buffer, sizeof(buffer), "%d", value), buffer);
}

bool SC::JsonStreamWriter::writeUint8(uint8_t value)
{
    char buffer[30];
    return writeValue(snprintf(buffer, sizeof(buffer), "%d", value), buffer);
}

bool SC::JsonStreamWriter::writeInt32(int32_t value)
{
    char buffer[30];
    return writeValue(snprintf(buffer, sizeof(buffer), "%d", value), buffer);
}

bool SC::JsonStreamWriter::writeUint32(uint32_t value)
{
    char buffer[30];
    return writeValue(snprintf(buffer, sizeof(buffer), "%d", value), buffer);
}

bool SC::JsonStreamWriter::writeInt64(int64_t value)
{
    char buffer[30];
    return writeValue(snprintf(buffer, sizeof(buffer), "%lld", value), buffer);
}

bool SC::JsonStreamWriter::writeUint64(uint64_t value)
{
    char buffer[30];
    return writeValue(snprintf(buffer, sizeof(buffer), "%lld", value), buffer);
}

bool SC::JsonStreamWriter::writeBoolean(bool value) { return writeValue(value ? 4 : 5, value ? "true" : "false"); }

bool SC::JsonStreamWriter::writeNull() { return writeValue(4, "null"); }

bool SC::JsonStreamWriter::writeString(StringView value)
{
    if (not onBeforeValue() or not writeSeparator())
    {
        return false;
    }
    // TODO: Escape json string
    if (output.write("\""_a8) and output.write(value) and output.write("\""_a8))
    {
        return onAfterValue();
    }
    return false;
}

bool SC::JsonStreamWriter::startArray()
{
    if (not onBeforeValue() or not writeSeparator())
    {
        return false;
    }
    if (output.write("["_a8))
    {
        return state.push_back(ArrayFirst);
    }
    return false;
}

bool SC::JsonStreamWriter::endArray()
{
    if (state.isEmpty() or ((state.back() != Array) and (state.back() != ArrayFirst)))
    {
        return false;
    }
    if (output.write("]"_a8))
    {
        if (state.pop_back())
        {
            return onAfterValue();
        }
    }
    return false;
}

bool SC::JsonStreamWriter::startObject()
{
    if (not onBeforeValue() or not writeSeparator())
    {
        return false;
    }

    if (output.write("{"_a8))
    {
        return state.push_back(ObjectFirst);
    }
    return false;
}

bool SC::JsonStreamWriter::endObject()
{
    if (state.isEmpty() or ((state.back() != Object) and (state.back() != ObjectFirst)))
    {
        return false;
    }
    if (output.write("}"_a8))
    {
        if (state.pop_back())
        {
            return onAfterValue();
        }
    }
    return false;
}

bool SC::JsonStreamWriter::objectFieldName(StringView name)
{
    if (state.isEmpty() or ((state.back() != Object) and (state.back() != ObjectFirst)))
    {
        return false;
    }
    if (state.back() == Object)
    {
        if (not output.write(","_a8))
        {
            return false;
        }
    }
    else
    {
        state.back() = Object;
    }
    // TODO: Escape JSON name
    if (output.write("\""_a8) and output.write(name) and output.write("\":"_a8))
    {
        return state.push_back(ObjectValue);
    }
    return false;
}
