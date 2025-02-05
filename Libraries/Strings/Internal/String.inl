// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Strings/String.h"

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
        memcpy(data.items, sv.bytesWithoutTerminator(), length + numZero);
    }
    else
    {
        if (length > 0)
        {
            memcpy(data.items, sv.bytesWithoutTerminator(), length);
        }
        for (size_t idx = 0; idx < numZero; ++idx)
        {
            data.items[length + idx] = 0;
        }
    }
    return true;
}

SC::StringView SC::String::view() const SC_LANGUAGE_LIFETIME_BOUND
{
    const bool  isEmpty = data.isEmpty();
    const char* items   = isEmpty ? nullptr : data.items;
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
