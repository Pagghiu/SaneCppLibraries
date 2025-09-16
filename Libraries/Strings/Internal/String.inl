// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Strings/String.h"
#include "../../Strings/StringFormat.h"

struct SC::String::Internal
{
    static void ensureZeroTermination(Buffer& buffer, StringEncoding encoding)
    {
        const size_t numZeros = StringEncodingGetSize(encoding);
        if (buffer.size() >= numZeros)
        {
            auto* data = buffer.data();
            for (size_t idx = 0; idx < numZeros; ++idx)
            {
                data[buffer.size() - 1 - idx] = 0;
            }
        }
    }
};

SC::String::String(Buffer&& otherData, StringEncoding encoding) : encoding(encoding)
{
    SC_ASSERT_RELEASE(data.assignMove(move(otherData)));
    Internal::ensureZeroTermination(data, encoding);
}

SC::String::String(StringEncoding encoding, uint32_t inlineCapacity) : encoding(encoding), data(inlineCapacity) {}

SC::String::String(Buffer&& otherData, StringEncoding encoding, uint32_t inlineCapacity)
    : String(encoding, inlineCapacity)
{
    SC_ASSERT_RELEASE(data.assignMove(move(otherData)));
    Internal::ensureZeroTermination(data, encoding);
}

bool SC::String::owns(StringSpan view) const
{
    return (view.bytesWithoutTerminator() >= this->view().bytesWithoutTerminator()) and
           (view.bytesWithoutTerminator() <= (this->view().bytesWithoutTerminator() + this->view().sizeInBytes()));
}

bool SC::String::assign(StringSpan sv)
{
    encoding             = sv.getEncoding();
    const size_t length  = sv.sizeInBytes();
    const size_t numZero = StringEncodingGetSize(encoding);
    if (not data.resizeWithoutInitializing(length + numZero))
        return false;
    if (sv.isNullTerminated())
    {
        memcpy(data.data(), sv.bytesWithoutTerminator(), length + numZero);
    }
    else
    {
        if (length > 0)
        {
            memcpy(data.data(), sv.bytesWithoutTerminator(), length);
        }
        auto* memory = data.data();
        for (size_t idx = 0; idx < numZero; ++idx)
        {
            memory[length + idx] = 0;
        }
    }
    return true;
}

SC::String& SC::String::operator=(StringSpan view)
{
    SC_ASSERT_RELEASE(assign(view));
    return *this;
}

SC::StringView SC::String::view() const SC_LANGUAGE_LIFETIME_BOUND
{
    const bool  isEmpty = data.isEmpty();
    const char* items   = isEmpty ? nullptr : data.data();
    return StringView({items, isEmpty ? 0 : data.size() - StringEncodingGetSize(encoding)}, not isEmpty, encoding);
}

SC::String::GrowableImplementation::GrowableImplementation(String& string, IGrowableBuffer::DirectAccess& da)
    : string(string), da(da)
{
    da = {string.data.size(), string.data.capacity(), string.data.data()};
}

SC::String::GrowableImplementation::~GrowableImplementation()
{
    if (string.data.size() != da.sizeInBytes)
    {
        (void)string.data.resizeWithoutInitializing(da.sizeInBytes);
    }
    const size_t numZeroes = StringEncodingGetSize(string.getEncoding());
    if (string.data.size() + numZeroes <= string.data.capacity())
    {
        // Add null-terminator
        (void)string.data.resize(string.data.size() + numZeroes, 0);
    }
}

bool SC::String::GrowableImplementation::tryGrowTo(size_t newSize)
{
    bool res = true;
    if (newSize > 0)
    {
        const size_t numZeroes = StringEncodingGetSize(string.getEncoding());
        res = string.data.reserve(newSize + numZeroes) and string.data.resizeWithoutInitializing(newSize);
    }
    else
    {
        string.data.clear();
    }
    da = {string.data.size(), string.data.capacity(), string.data.data()};
    return res;
}

namespace SC
{
bool StringFormatterFor<String>::format(StringFormatOutput& data, const StringView specifier, const String& value)
{
    return StringFormatterFor<StringView>::format(data, specifier, value.view());
}
} // namespace SC
