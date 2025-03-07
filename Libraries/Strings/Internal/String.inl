// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Strings/String.h"
#include "../../Strings/StringConverter.h" // ensureZeroTermination

SC::String::String(Buffer&& otherData, StringEncoding encoding) : encoding(encoding)
{
    SC_ASSERT_RELEASE(data.assignMove(move(otherData)));
    StringConverter::ensureZeroTermination(data, encoding);
}

SC::String::String(StringEncoding encoding, uint32_t inlineCapacity) : encoding(encoding), data(inlineCapacity) {}

SC::String::String(Buffer&& otherData, StringEncoding encoding, uint32_t inlineCapacity)
    : String(encoding, inlineCapacity)
{
    SC_ASSERT_RELEASE(data.assignMove(move(otherData)));
    StringConverter::ensureZeroTermination(data, encoding);
}

bool SC::String::owns(StringView view) const
{
    return (view.bytesWithoutTerminator() >= this->view().bytesWithoutTerminator()) and
           (view.bytesWithoutTerminator() <= (this->view().bytesWithoutTerminator() + this->view().sizeInBytes()));
}

bool SC::String::assign(StringView sv)
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

SC::String& SC::String::operator=(StringView view)
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

SC::native_char_t* SC::String::nativeWritableBytesIncludingTerminator()
{
#if SC_PLATFORM_WINDOWS
    SC_ASSERT_RELEASE(encoding == StringEncoding::Utf16);
    return reinterpret_cast<wchar_t*>(data.data());
#else
    SC_ASSERT_RELEASE(encoding < StringEncoding::Utf16);
    return data.data();
#endif
}
