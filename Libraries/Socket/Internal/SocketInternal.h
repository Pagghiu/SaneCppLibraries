#pragma once

#include "../../Foundation/Span.h"

namespace SC
{
namespace detail
{
template <size_t N>
[[nodiscard]] bool writeNullTerminatedToBuffer(Span<const char> source, char (&destination)[N])
{
    if (N < source.sizeInBytes() + 1)
        return false;
    ::memcpy(destination, source.data(), source.sizeInBytes());
    destination[source.sizeInBytes()] = 0;
    return true;
}

/// @brief Bitwise copies contents of this Span over another (non-overlapping)
template <typename T, typename U>
[[nodiscard]] bool copyFromTo(const Span<T> source, Span<U>& other)
{
    if (other.sizeInBytes() < source.sizeInBytes())
        return false;
    ::memcpy(other.data(), source.data(), source.sizeInBytes());
    other = {other.data(), source.sizeInBytes() / sizeof(U)};
    return true;
}
} // namespace detail

#if !SC_PLATFORM_WINDOWS
constexpr int SOCKET_ERROR = -1;
#endif

} // namespace SC
