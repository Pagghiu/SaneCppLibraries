#pragma once
#include "Language.h"
#include "LibC.h"
#include "Types.h"

namespace SC
{
enum class Comparison
{
    Smaller = -1,
    Equals  = 0,
    Bigger  = 1
};

template <typename Type, typename Size = SC::size_t>
struct Span
{
    typedef typename SameConstnessAs<Type, uint8_t>::type ByteType;

    Type* data;
    Size  size;
    constexpr Span() : data(nullptr), size(0) {}
    constexpr Span(Type* data, Size size) : data(data), size(size) {}

    constexpr bool               isNull() const { return data == nullptr; }
    [[nodiscard]] constexpr bool viewAt(Size offset, Size length, Span& other)
    {
        if (offset + length <= size)
        {
            other = Span(static_cast<ByteType*>(data) + offset, length);
            return true;
        }
        else
        {
            return false;
        }
    }

    // TODO: Remove Span<void>::advance
    [[nodiscard]] constexpr bool advance(Size length)
    {
        if (length <= size)
        {
            data = static_cast<ByteType*>(data) + length;
            size -= length;
            return true;
        }
        else
        {
            return false;
        }
    }

    // TODO: Remove Span<void>::readAndAdvance
    template <typename T>
    [[nodiscard]] constexpr bool readAndAdvance(T& value)
    {
        const Size length = sizeof(T);
        if (length <= size)
        {
            memcpy(&value, data, length);
            data = static_cast<ByteType*>(data) + length;
            size -= length;
            return true;
        }
        else
        {
            return false;
        }
    }

    template <typename DestinationType>
    constexpr Span<DestinationType> castTo()
    {
        return Span<DestinationType>(static_cast<DestinationType*>(data), size);
    }
    [[nodiscard]] bool equalsContent(Span other) const
    {
        return size == other.size ? memcmp(data, other.data, size) == 0 : false;
    }
    [[nodiscard]] Comparison compare(Span other) const
    {
        const int res = memcmp(data, other.data, min(size, other.size));
        if (res < 0)
            return Comparison::Smaller;
        else if (res == 0)
            return Comparison::Equals;
        else
            return Comparison::Bigger;
    }
    [[nodiscard]] bool insertCopy(size_t idx, const Type* source, Size sourceSize)
    {
        if (sourceSize + idx <= size)
        {
            memcpy(data + idx, source, sourceSize * sizeof(Type));
            return true;
        }
        return false;
    }
    [[nodiscard]] bool copyTo(Span<void> other)
    {
        if (other.size >= size)
        {
            memcpy(other.data, data, size);
            return true;
        }
        return false;
    }
};
} // namespace SC
