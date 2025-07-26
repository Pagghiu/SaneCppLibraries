// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Segment.inl"
namespace SC
{
// clang-format off
#if SC_COMPILER_MSVC && !SC_LANGUAGE_CPP_AT_LEAST_17
template <typename T> [[nodiscard]] constexpr T* launder(T* p) noexcept { return p; } // We're in UB-land
#else
template <typename T> [[nodiscard]] constexpr T* launder(T* p) noexcept { return __builtin_launder(p); }
#endif
// TODO: Not a fully compliant implementation, replace it with a proper one under C++23
// https://stackoverflow.com/questions/79164176/emulate-stdstart-lifetime-as-array-in-c20
template <typename T> [[nodiscard]] T* start_lifetime_as_array(Span<T> span) noexcept { return span.empty() ? static_cast<T*>(span.data()) : launder(static_cast<T*>(::memmove(span.data(), span.data(), sizeof(T) * span.sizeInElements()))); }

template <typename T, typename U> [[nodiscard]] const T* start_lifetime_as(Span<const U> span) noexcept { void* p = const_cast<void*>(static_cast<const void*>(span.data())); return launder(static_cast<const T*>(::memmove(p, p, sizeof(T)))); }
// clang-format on
} // namespace SC
template <typename T>
inline void SC::detail::SegmentTrivial<T>::destruct(Span<T>) noexcept
{}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::copyConstructAs(Span<T> data, Span<const U> value) noexcept
{
    const size_t numElements = data.sizeInElements();
    T*           destData    = start_lifetime_as_array(data);
    if (value.sizeInBytes() == 1)
    {
        int intValue = 0;
        ::memcpy(&intValue, value.data(), 1);
        ::memset(destData, intValue, data.sizeInBytes());
    }
    else
    {
        const size_t valueSize   = value.sizeInBytes();
        const auto*  sourceValue = value.data();
        for (size_t idx = 0; idx < numElements; idx++)
        {
            ::memcpy(destData + idx, sourceValue, valueSize);
        }
    }
}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::copyConstruct(Span<T> data, const U* src) noexcept
{
    ::memmove(start_lifetime_as_array(data), src, data.sizeInBytes());
}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::copyAssign(Span<T> data, const U* src) noexcept
{
    ::memcpy(data.data(), src, data.sizeInBytes());
}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::copyInsert(Span<T> data, Span<const U> values) noexcept
{
    ::memmove(data.template reinterpret_as_span_of<char>().data() + values.sizeInBytes(), data.data(),
              data.sizeInBytes());
    ::memmove(data.data(), values.data(), values.sizeInBytes());
}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::moveConstruct(Span<T> data, U* src) noexcept
{
    ::memcpy(start_lifetime_as_array(data), src, data.sizeInBytes());
}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::moveAssign(Span<T> data, U* src) noexcept
{
    ::memcpy(start_lifetime_as_array(data), src, data.sizeInBytes());
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::remove(Span<T> data, size_t numElements) noexcept
{
    ::memmove(data.data(), data.template reinterpret_as_span_of<char>().data() + numElements, numElements);
}
