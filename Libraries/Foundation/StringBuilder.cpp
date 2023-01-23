// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include <stdarg.h> // va_list
#include <stdio.h>  // vsnprintf

#include "StringBuilder.h"

bool SC::StringBuilder::assignStringView(StringView sv)
{
    bool         res;
    const size_t length = sv.sizeInBytes();
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
