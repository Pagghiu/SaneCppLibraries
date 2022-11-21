// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "String.h"
#include <stdarg.h> // va_list
#include <stdio.h>  // vsnprintf

bool SC::String::assignStringView(StringView sv)
{
    bool         res;
    const size_t length = sv.sizeInBytesWithoutTerminator();
    res                 = data.resizeWithoutInitializing(length + 1);
    if (sv.isNullTerminated())
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length + 1);
    }
    else
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length);
        data.items[length] = 0;
    }
    return res;
}

bool SC::text::StringFormatterFor<SC::String>::format(Vector<char_t>& data, const StringIteratorASCII specifier,
                                                      const SC::String value)
{
    return StringFormatterFor<SC::StringView>::format(data, specifier, value.view());
}
