// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_I_GROWABLE_BUFFER_SPAN_DEFINITION_H
#if SC_FOUNDATION_I_GROWABLE_BUFFER_SPAN_DEFINITION_H != 1
#error "IGrowableBufferSpan.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_I_GROWABLE_BUFFER_SPAN_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "IGrowableBuffer.h"
#include "Span.h"

namespace SC
{
template <>
struct GrowableBuffer<Span<char>> : public IGrowableBuffer
{
    Span<char>& content;
    size_t      capacity;
    bool        setContentInDestructor = true;
    GrowableBuffer(Span<char>& span)
        : content(span), capacity(content.sizeInBytes()), IGrowableBuffer(&GrowableBuffer::fixedGrowTo)
    {
        IGrowableBuffer::directAccess = {span.sizeInBytes(), capacity, span.data()};
    }
    GrowableBuffer(Span<char>& span, size_t capacity)
        : content(span), capacity(capacity), IGrowableBuffer(&GrowableBuffer::fixedGrowTo)
    {
        IGrowableBuffer::directAccess = {span.sizeInBytes(), capacity, span.data()};
    }
    ~GrowableBuffer()
    {
        if (setContentInDestructor)
            content = {content.data(), IGrowableBuffer::directAccess.sizeInBytes};
    }

    static bool fixedGrowTo(IGrowableBuffer& growableBuffer, size_t newSize)
    {
        GrowableBuffer& buffer = static_cast<GrowableBuffer&>(growableBuffer);
        if (newSize < static_cast<GrowableBuffer&>(growableBuffer).capacity)
        {
            buffer.content = {buffer.content.data(), newSize};
            return true;
        }
        return false;
    }
};
} // namespace SC
#endif
