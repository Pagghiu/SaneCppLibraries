// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Internal/IGrowableBuffer.h"
#include "../Foundation/StringSpan.h"

namespace SC
{

namespace detail
{
/// @brief A fixed size native string that converts to a StringSpan
template <int N>
struct StringNativeFixed
{
    size_t        length    = 0;
    native_char_t buffer[N] = {};

    [[nodiscard]] auto view() const { return StringSpan({buffer, length}, true, StringEncoding::Native); }
    [[nodiscard]] bool isEmpty() const { return length == 0; }
    [[nodiscard]] auto writableSpan() { return Span<native_char_t>{buffer, N}; }
    [[nodiscard]] bool operator==(StringSpan other) const { return view() == other; }

    void clear()
    {
        length    = 0;
        buffer[0] = 0;
    }
    [[nodiscard]] bool assign(StringSpan str)
    {
        clear();
        return append(str);
    }

    [[nodiscard]] bool append(StringSpan str)
    {
        StringSpan::NativeWritable string = {{buffer, N}, length};
        if (not str.appendNullTerminatedTo(string))
            return false;
        length = string.length;
        return true;
    }
};
} // namespace detail

template <int N>
using StringNativeBuffer = detail::StringNativeFixed<N>;

/// @brief Pre-sized char array holding enough space to represent a file system path
struct SC_FOUNDATION_EXPORT StringPath
{
    /// @brief Maximum size of paths on current native platform
#if SC_PLATFORM_WINDOWS
    static constexpr size_t MaxPath = 1024;
#elif SC_PLATFORM_APPLE
    static constexpr size_t MaxPath = 1024; // Equal to 'PATH_MAX' on macOS
#else
    static constexpr size_t MaxPath = 4096; // Equal to 'PATH_MAX' on Linux
#endif
    static constexpr size_t      StorageCapacity = MaxPath + 1;
    [[nodiscard]] StringSpan     view() const { return path.view(); }
    [[nodiscard]] StringEncoding getEncoding() const { return StringEncoding::Native; }

    [[nodiscard]] bool isEmpty() const { return path.view().isEmpty(); }
    [[nodiscard]] bool append(StringSpan str) { return path.append(str); }
    [[nodiscard]] bool assign(StringSpan str) { return path.assign(str); }
    [[nodiscard]] bool resize(size_t newSize);

    [[nodiscard]] Span<native_char_t> writableSpan() { return path.writableSpan(); }

  private:
    detail::StringNativeFixed<StorageCapacity> path;
};

template <>
struct SC_FOUNDATION_EXPORT GrowableBuffer<StringPath> : public IGrowableBuffer
{
    StringPath& sp;
    GrowableBuffer(StringPath& string) noexcept;
    ~GrowableBuffer() noexcept;
    static bool tryGrowTo(IGrowableBuffer& gb, size_t newSize) noexcept;
    static auto getEncodingFor(const StringPath& sp) noexcept { return sp.getEncoding(); }
    void        finalize() noexcept;
};
} // namespace SC
