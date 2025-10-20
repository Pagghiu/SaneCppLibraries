// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/StringPath.h"

SC::GrowableBuffer<SC::StringPath>::GrowableBuffer(StringPath& string) noexcept
    : IGrowableBuffer(&GrowableBuffer::tryGrowTo), sp(string)
{
    directAccess = {sp.view().sizeInBytes(), (StringPath::MaxPath - 1) * sizeof(native_char_t),
                    sp.writableSpan().data()};
}

SC::GrowableBuffer<SC::StringPath>::~GrowableBuffer() noexcept { finalize(); }

void SC::GrowableBuffer<SC::StringPath>::finalize() noexcept
{
    (void)sp.resize(directAccess.sizeInBytes / sizeof(native_char_t));
}

bool SC::GrowableBuffer<SC::StringPath>::tryGrowTo(IGrowableBuffer& gb, size_t newSize) noexcept
{
    GrowableBuffer& self          = static_cast<GrowableBuffer&>(gb);
    const bool      res           = self.sp.resize(newSize / sizeof(native_char_t));
    self.directAccess.sizeInBytes = self.sp.view().sizeInBytes();
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
