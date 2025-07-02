// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/PrimitiveTypes.h"
namespace SC
{
/// @brief Virtual interface to abstract a linear binary buffer.
/// It's used by File library to read data into a Buffer or String object without need to know their types.
/// This allows breaking the dependency between File and Strings / Memory libraries.
struct IGrowableBuffer
{
    virtual ~IGrowableBuffer() {}

    struct DirectAccess
    {
        size_t sizeInBytes     = 0;
        size_t capacityInBytes = 0;
        void*  data            = nullptr;
    };

    /// @brief Obtain direct access to buffer data
    [[nodiscard]] virtual DirectAccess getDirectAccess() = 0;

    /// @brief  Try to grow the buffer to a new size.
    /// @param newSize The desired new size for the buffer.
    /// @return `true` if the buffer was successfully grown, `false` otherwise.
    [[nodiscard]] virtual bool tryGrowTo(size_t newSize) = 0;
};

/// @brief Partial specialize GrowableBuffer deriving from IGrowableBuffer (see how String and Buffer do it)
template <typename T>
struct GrowableBuffer;

} // namespace SC
