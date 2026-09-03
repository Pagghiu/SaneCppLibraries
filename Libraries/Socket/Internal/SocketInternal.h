#pragma once

#include "../../Common/StringSpan.h"
#include <memory.h> // memcpy

namespace SC
{
namespace detail
{
[[nodiscard]] constexpr bool isASCII(StringSpan text)
{
    if (text.getEncoding() == StringEncoding::Utf16)
        return false;

    const Span<const char> bytes = text.toCharSpan();
    for (size_t idx = 0; idx < bytes.sizeInBytes(); ++idx)
    {
        if (static_cast<unsigned char>(bytes[idx]) > 0x7f)
            return false;
    }
    return true;
}

template <size_t N>
[[nodiscard]] bool writeNullTerminatedToBuffer(Span<const char> source, char (&destination)[N])
{
    SC_TRY(N >= source.sizeInBytes() + 1);
    ::memcpy(destination, source.data(), source.sizeInBytes());
    destination[source.sizeInBytes()] = 0;
    return true;
}

/// @brief Bitwise copies contents of this Span over another (non-overlapping)
template <typename T, typename U>
[[nodiscard]] bool copyFromTo(const Span<T> source, Span<U>& other)
{
    SC_TRY(other.sizeInBytes() >= source.sizeInBytes());
    ::memcpy(other.data(), source.data(), source.sizeInBytes());
    other = {other.data(), source.sizeInBytes() / sizeof(U)};
    return true;
}
} // namespace detail

#if !SC_PLATFORM_WINDOWS
constexpr int SOCKET_ERROR = -1;
#endif

} // namespace SC
