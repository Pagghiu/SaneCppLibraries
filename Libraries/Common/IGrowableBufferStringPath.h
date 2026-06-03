// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_I_GROWABLE_BUFFER_STRING_PATH_DEFINITION_H
#if SC_FOUNDATION_I_GROWABLE_BUFFER_STRING_PATH_DEFINITION_H != 1
#error "IGrowableBufferStringPath.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_I_GROWABLE_BUFFER_STRING_PATH_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "IGrowableBuffer.h"
#include "StringPath.h"

namespace SC
{
template <>
struct SC_FOUNDATION_EXPORT GrowableBuffer<StringPath> : public IGrowableBuffer
{
    StringPath& sp;

    GrowableBuffer(StringPath& string) noexcept : IGrowableBuffer(&GrowableBuffer::tryGrowTo), sp(string)
    {
        directAccess = {sp.view().sizeInBytes(), StringPath::MaxPath * sizeof(native_char_t), sp.writableSpan().data()};
    }

    ~GrowableBuffer() noexcept { finalize(); }

    static bool tryGrowTo(IGrowableBuffer& gb, size_t newSize) noexcept
    {
        GrowableBuffer& self          = static_cast<GrowableBuffer&>(gb);
        const bool      res           = self.sp.resize(newSize / sizeof(native_char_t));
        self.directAccess.sizeInBytes = self.sp.view().sizeInBytes();
        return res;
    }

    static auto getEncodingFor(const StringPath& sp) noexcept { return sp.getEncoding(); }

    void finalize() noexcept { (void)sp.resize(directAccess.sizeInBytes / sizeof(native_char_t)); }
};
} // namespace SC

#endif // SC_FOUNDATION_I_GROWABLE_BUFFER_STRING_PATH_DEFINITION_H
