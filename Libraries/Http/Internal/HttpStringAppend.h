// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Internal/IGrowableBuffer.h"

namespace SC
{
struct HttpStringAppend : public IGrowableBuffer
{
    size_t size() const { return directAccess.sizeInBytes; }
    char*  data() const { return static_cast<char*>(directAccess.data); }

    void clear() { (void)resizeWithoutInitializing(0); }

    [[nodiscard]] bool append(StringSpan span) { return append(span.toCharSpan(), 0); }

    [[nodiscard]] bool append(Span<const char> span, size_t extraZeroes)
    {
        const size_t oldSize = size();
        const size_t newSize = oldSize + span.sizeInBytes() + extraZeroes;
        SC_TRY(resizeWithoutInitializing(newSize));
        if (not span.empty())
        {
            ::memcpy(data() + oldSize, span.data(), span.sizeInBytes());
        }
        if (extraZeroes > 0)
        {
            ::memset(data() + oldSize + span.sizeInBytes(), 0, extraZeroes);
        }
        return true;
    }
};
} // namespace SC
