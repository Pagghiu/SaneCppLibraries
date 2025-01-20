// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Span.h"
namespace SC
{
struct HeapBuffer;
}
//! @addtogroup group_foundation_utility
//! @{

/// @brief A move-only owned buffer of bytes
struct SC::HeapBuffer
{
    Span<char> data;

    HeapBuffer();
    ~HeapBuffer();

    HeapBuffer(HeapBuffer&& other);
    HeapBuffer(const HeapBuffer& other) = delete;

    HeapBuffer& operator=(const HeapBuffer& other) = delete;
    HeapBuffer& operator=(HeapBuffer&& other);

    /// @brief Allocates numBytes releasing previously allocated memory
    [[nodiscard]] bool allocate(size_t numBytes);

    /// @brief Allocates numBytes coping the existing contents of previously allocated memory
    [[nodiscard]] bool reallocate(size_t numBytes);

    /// @brief Releases any previously allocated memory
    void release();
};

//! @}
