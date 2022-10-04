#pragma once
#include "libc.h"
#include "types.h"

namespace sanecpp
{
template <typename Type, typename Size = sanecpp::size_t>
struct span
{
    Type* data;
    Size  size;
    constexpr span() : data(nullptr), size(0) {}
    constexpr span(Type* data, Size size) : data(data), size(size) {}
    bool equalsContent(span other) const { return size == other.size ? memcmp(data, other.data, size) == 0 : false; }
};
} // namespace sanecpp
