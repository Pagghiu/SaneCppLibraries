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
    Type* data;
    Size  size;
    constexpr Span() : data(nullptr), size(0) {}
    constexpr Span(Type* data, Size size) : data(data), size(size) {}
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
};
} // namespace SC
