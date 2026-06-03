// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_I_GROWABLE_BUFFER_DEFINITION_H
#if SC_FOUNDATION_I_GROWABLE_BUFFER_DEFINITION_H != 1
#error "IGrowableBuffer.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_I_GROWABLE_BUFFER_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "CompilerMacrosExport.h"
#include "PrimitiveDefinitions.h"

namespace SC
{
/// @brief Interface abstracting a linear binary buffer.
/// It's used by File library to read data into a Buffer or String object without need to know their types.
/// This allows breaking the dependency between File and Strings / Memory libraries.
struct SC_FOUNDATION_EXPORT IGrowableBuffer
{
    using TryGrowFunc = bool (*)(IGrowableBuffer& growableBuffer, size_t newSize);

    struct DirectAccess
    {
        size_t sizeInBytes     = 0;
        size_t capacityInBytes = 0;
        void*  data            = nullptr;
    };

    IGrowableBuffer(TryGrowFunc func) : ptrTryGrowTo(func) {}

    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        if (newSize <= directAccess.capacityInBytes) // try to avoid a virtual call
        {
            directAccess.sizeInBytes = newSize; // size on type erased buffer will be set by virtual destructor
            return true;
        }
        return ptrTryGrowTo(*this, newSize);
    }

    void   clear() { directAccess.sizeInBytes = 0; }
    char*  data() const { return static_cast<char*>(directAccess.data); }
    size_t size() const { return directAccess.sizeInBytes; }
    auto   getDirectAccess() const { return directAccess; }

  protected:
    TryGrowFunc  ptrTryGrowTo;
    DirectAccess directAccess;
};

/// @brief Partial specialize GrowableBuffer deriving from IGrowableBuffer (see how Span, String and Buffer do it)
template <typename T>
struct GrowableBuffer
{
    T& content;
    GrowableBuffer(T& content) : content(content) {}
};

} // namespace SC
#endif
