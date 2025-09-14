// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/PrimitiveTypes.h"
namespace SC
{
/// @brief Virtual interface to abstract a linear binary buffer.
/// It's used by File library to read data into a Buffer or String object without need to know their types.
/// This allows breaking the dependency between File and Strings / Memory libraries.
struct SC_COMPILER_EXPORT IGrowableBuffer
{
    struct DirectAccess
    {
        size_t sizeInBytes     = 0;
        size_t capacityInBytes = 0;
        void*  data            = nullptr;
    };

    /// Note: derived classes should set their size == to match directAccess.sizeInBytes during destructor
    virtual ~IGrowableBuffer() = default;

    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        if (newSize < directAccess.capacityInBytes) // try to avoid a virtual call
        {
            directAccess.sizeInBytes = newSize; // size on type erased buffer will be set by virtual destructor
            return true;
        }
        return tryGrowTo(newSize);
    }

    DirectAccess getDirectAccess() const { return directAccess; }

  protected:
    /// @brief  Try to grow the buffer to a new size, and update directAccess.
    /// @param newSize The desired new size for the buffer.
    /// @return `true` if the buffer was successfully grown, `false` otherwise.
    [[nodiscard]] virtual bool tryGrowTo(size_t newSize) = 0;

    DirectAccess directAccess;
};

/// @brief Partial specialize GrowableBuffer deriving from IGrowableBuffer (see how String and Buffer do it)
template <typename T>
struct GrowableBuffer;

} // namespace SC
