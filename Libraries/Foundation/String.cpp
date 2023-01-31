// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "String.h"

bool SC::String::assignStringView(StringView sv)
{
    const size_t length = sv.sizeInBytes();
    bool         res    = data.resizeWithoutInitializing(length + 1);
    if (sv.isNullTerminated())
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length + 1);
    }
    else
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length);
        data.items[length] = 0;
    }
    encoding = sv.getEncoding();
    return res;
}

bool SC::StringFormatterFor<SC::String>::format(StringFormatOutput& data, const StringIteratorASCII specifier,
                                                const SC::String& value)
{
    return StringFormatterFor<SC::StringView>::format(data, specifier, value.view());
}
