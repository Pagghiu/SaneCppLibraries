// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Segment.inl"
namespace SC
{
namespace detail
{
// clang-format off
#if SC_COMPILER_MSVC && !SC_LANGUAGE_CPP_AT_LEAST_17
template <typename T> [[nodiscard]] constexpr T* launder(T* p) noexcept { return p; } // We're in UB-land
#else
template <typename T> [[nodiscard]] constexpr T* launder(T* p) noexcept { return __builtin_launder(p); }
#endif
// clang-format on

// TODO: Not a fully compliant implementation, replace it with the actual one under C++23
// https://stackoverflow.com/questions/79164176/emulate-stdstart-lifetime-as-array-in-c20
template <class T>
T* start_lifetime_as_array(void* p, size_t n) noexcept
{
    if (n == 0)
        return static_cast<T*>(p);
    else
        return launder(static_cast<T*>(::memmove(p, p, sizeof(T) * n)));
}
} // namespace detail
} // namespace SC
template <typename T>
inline void SC::detail::SegmentTrivial<T>::destruct(Span<T>)
{}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::copyConstructAs(Span<T> data, Span<const T> value)
{
    const size_t numElements = data.sizeInElements();
    T*           destData    = start_lifetime_as_array<T>(data.data(), numElements);
    if (value.sizeInBytes() == 1)
    {
        int intValue = 0;
        ::memcpy(&intValue, value.data(), 1);
        ::memset(destData, intValue, data.sizeInBytes());
    }
    else
    {
        const size_t valueSize   = value.sizeInBytes();
        const T*     sourceValue = value.data();
        for (size_t idx = 0; idx < numElements; idx++)
        {
            ::memcpy(destData + idx, sourceValue, valueSize);
        }
    }
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::copyConstruct(Span<T> data, const T* src)
{
    ::memmove(start_lifetime_as_array<T>(data.data(), data.sizeInElements()), src, data.sizeInBytes());
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::copyAssign(Span<T> data, const T* src)
{
    ::memcpy(data.data(), src, data.sizeInBytes());
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::copyInsert(Span<T> data, Span<const T> values)
{
    ::memmove(data.template reinterpret_as_array_of<char>().data() + values.sizeInBytes(), data.data(),
              data.sizeInBytes());
    ::memmove(data.data(), values.data(), values.sizeInBytes());
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::moveConstruct(Span<T> data, T* src)
{
    ::memcpy(start_lifetime_as_array<T>(data.data(), data.sizeInElements()), src, data.sizeInBytes());
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::moveAssign(Span<T> data, T* src)
{
    ::memcpy(start_lifetime_as_array<T>(data.data(), data.sizeInElements()), src, data.sizeInBytes());
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::remove(Span<T> data, size_t numElements)
{
    ::memmove(data.data(), data.template reinterpret_as_array_of<char>().data() + numElements, numElements);
}
