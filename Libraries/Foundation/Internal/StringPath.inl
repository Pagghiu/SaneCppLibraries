// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/StringPath.h"

SC::GrowableBuffer<SC::StringPath>::GrowableBuffer(StringPath& string) : sp(string)
{
    directAccess = {sp.view().sizeInBytes(), (StringPath::MaxPath - 1) * sizeof(native_char_t),
                    sp.writableSpan().data()};
}

SC::GrowableBuffer<SC::StringPath>::~GrowableBuffer()
{
    (void)sp.resize(directAccess.sizeInBytes / sizeof(native_char_t));
}

bool SC::GrowableBuffer<SC::StringPath>::tryGrowTo(size_t newSize)
{
    const bool res           = sp.resize(newSize / sizeof(native_char_t));
    directAccess.sizeInBytes = sp.view().sizeInBytes();
    return res;
}

bool SC::StringPath::resize(size_t newSize)
{
    if (newSize + 1 < MaxPath)
    {
        path.length              = newSize;
        path.buffer[path.length] = 0;
    }
    return newSize + 1 < MaxPath;
}
