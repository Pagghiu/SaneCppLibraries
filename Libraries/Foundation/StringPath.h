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
    size_t        length = 0;
    native_char_t buffer[N];

    [[nodiscard]] auto view() const { return StringSpan({buffer, length}, true, StringEncoding::Native); }
    [[nodiscard]] bool isEmpty() const { return length == 0; }
    [[nodiscard]] auto writableSpan() { return Span<native_char_t>{buffer, N}; }
    [[nodiscard]] bool operator==(StringSpan other) const { return view() == other; }
    [[nodiscard]] bool assign(StringSpan str)
    {
        length = 0;
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

/// @brief Pre-sized char array holding enough space to represent a file system path
struct SC_COMPILER_EXPORT StringPath
{
    /// @brief Maximum size of paths on current native platform
#if SC_PLATFORM_WINDOWS
    static constexpr size_t MaxPath = 260; // Equal to 'MAX_PATH' on Windows
#elif SC_PLATFORM_APPLE
    static constexpr size_t MaxPath = 1024; // Equal to 'PATH_MAX' on macOS
#else
    static constexpr size_t MaxPath = 4096; // Equal to 'PATH_MAX' on Linux
#endif
    [[nodiscard]] StringSpan     view() const { return path.view(); }
    [[nodiscard]] StringEncoding getEncoding() const { return StringEncoding::Native; }

    [[nodiscard]] bool isEmpty() const { return path.view().isEmpty(); }
    [[nodiscard]] bool append(StringSpan str) { return path.append(str); }
    [[nodiscard]] bool assign(StringSpan str) { return path.assign(str); }
    [[nodiscard]] bool resize(size_t newSize);

    [[nodiscard]] Span<native_char_t> writableSpan() { return path.writableSpan(); }

  private:
    detail::StringNativeFixed<MaxPath> path;
};

template <>
struct SC_COMPILER_EXPORT GrowableBuffer<StringPath> final : public IGrowableBuffer
{
    StringPath& sp;
    GrowableBuffer(StringPath& string);
    virtual ~GrowableBuffer() override;
    virtual bool tryGrowTo(size_t newSize) override;
};
} // namespace SC
