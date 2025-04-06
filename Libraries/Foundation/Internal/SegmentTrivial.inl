// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Segment.inl"
template <typename T>
inline void SC::detail::SegmentTrivial<T>::destruct(Span<T>) noexcept
{}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::copyConstructAs(Span<T> data, Span<const U> value) noexcept
{
    const size_t numElements = data.sizeInElements();
    T*           destData    = data.template start_lifetime_as_array<T>();
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
    ::memmove(data.template start_lifetime_as_array<T>(), src, data.sizeInBytes());
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
    ::memcpy(data.template start_lifetime_as_array<T>(), src, data.sizeInBytes());
}

template <typename T>
template <typename U>
inline void SC::detail::SegmentTrivial<T>::moveAssign(Span<T> data, U* src) noexcept
{
    ::memcpy(data.template start_lifetime_as_array<T>(), src, data.sizeInBytes());
}

template <typename T>
inline void SC::detail::SegmentTrivial<T>::remove(Span<T> data, size_t numElements) noexcept
{
    ::memmove(data.data(), data.template reinterpret_as_span_of<char>().data() + numElements, numElements);
}
