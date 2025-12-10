// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Span.h"

namespace SC
{
/// @brief A fixed size circular queue (also known as ring buffer)
/// @note Uses only up to N-1 element slots of the passed in Span
template <typename T>
struct SC_COMPILER_EXPORT CircularQueue
{
    CircularQueue() = default;
    CircularQueue(Span<T> buffer) : buffer(buffer) {}

    [[nodiscard]] bool isEmpty() const { return readIndex == writeIndex; }

    [[nodiscard]] bool pushBack(T&& request) { return pushBack(request); }

    [[nodiscard]] bool pushBack(T& request)
    {
        const uint32_t nextWriteIndex = (writeIndex + 1) % buffer.sizeInElements();
        if (nextWriteIndex == readIndex)
            SC_LANGUAGE_UNLIKELY
            {
                return false; // Ring is full
            }
        buffer[writeIndex] = request;
        writeIndex         = nextWriteIndex;
        return true;
    }

    [[nodiscard]] bool popFront(T& request)
    {
        if (isEmpty())
            SC_LANGUAGE_UNLIKELY
            {
                return false; // Ring is empty
            }
        request   = move(buffer[readIndex]);
        readIndex = (readIndex + 1) % buffer.sizeInElements();
        return true;
    }

    [[nodiscard]] bool pushFront(T& request)
    {
        const uint32_t nextReadIndex =
            readIndex == 0 ? static_cast<uint32_t>(buffer.sizeInElements()) - 1 : readIndex - 1;
        if (nextReadIndex == writeIndex)
            SC_LANGUAGE_UNLIKELY
            {
                return false; // Ring is full
            }
        buffer[nextReadIndex] = move(request);
        readIndex             = nextReadIndex;
        return true;
    }

  private:
    Span<T>  buffer;
    uint32_t readIndex  = 0;
    uint32_t writeIndex = 0;
};
} // namespace SC
