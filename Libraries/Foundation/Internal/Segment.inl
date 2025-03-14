// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Assert.h"
#include "../../Foundation/Memory.h"
#include "../../Foundation/Segment.h"

template <typename VTable>
struct SC::Segment<VTable>::Internal
{
    static void releaseMemory(const SegmentHeader&, void* memory)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (not VTable::IsArray)
        {
            Memory::release(memory);
        }
    }

    static void* allocateMemory(const SegmentHeader&, size_t capacityBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (not VTable::IsArray)
        {
            return Memory::allocate(capacityBytes);
        }
        return nullptr;
    }

    static void* reallocateMemory(const SegmentHeader&, void* data, size_t capacityBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (not VTable::IsArray)
        {
            return Memory::reallocate(data, capacityBytes);
        }
        return nullptr;
    }

    static T* allocate(Segment& segment, size_t capacityBytes) noexcept
    {
        void* newData = Internal::allocateMemory(segment.header, capacityBytes);
        if (newData)
        {
            segment.header.capacityBytes = static_cast<uint32_t>(capacityBytes);
        }
        return static_cast<T*>(newData);
    }

    static T* reallocate(Segment& segment, size_t capacityBytes) noexcept
    {
        Span<T> selfSpan = segment.toSpan();
        T*      newData  = nullptr;
        if SC_LANGUAGE_IF_CONSTEXPR (not VTable::IsArray)
        {
            if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            {
                newData =
                    reinterpret_cast<T*>(Internal::reallocateMemory(segment.header, selfSpan.data(), capacityBytes));
            }
            else
            {
                // TODO: Room for optimization for memcpy-able objects (a >= subset than trivially copyable)
                newData = reinterpret_cast<T*>(Internal::allocateMemory(segment.header, capacityBytes));
                if (newData != nullptr)
                {
                    VTable::template moveConstruct<T>({newData, selfSpan.sizeInElements()}, selfSpan.data());
                    VTable::destruct(selfSpan);
                    Internal::releaseMemory(segment.header, selfSpan.data());
                }
            }
        }
        segment.header.capacityBytes = static_cast<uint32_t>(capacityBytes);
        return static_cast<T*>(newData);
    }

    static void releaseInternal(Segment& segment) noexcept
    {
        VTable::destruct(segment.toSpan());
        segment.header.sizeBytes = 0;
        if (not segment.isInline())
        {
            Internal::releaseMemory(segment.header, segment.data());
            Internal::eventuallyRestoreInlineData(segment);
        }
    }

    template <typename VTable2>
    static void eventuallyRestoreInlineData(Segment<VTable2>& segment) noexcept
    {
        const bool hasInlineData = segment.header.hasInlineData;
        segment.setData(hasInlineData ? segment.getInlineData() : nullptr);
        segment.header.capacityBytes = hasInlineData ? static_cast<uint32_t>(segment.getInlineCapacity()) : 0;
    }

    template <typename Construct, typename Assign, typename U>
    static bool assignInternal(Construct constructFunc, Assign assignFunc, Segment& segment, Span<U> span) noexcept
    {
        const size_t newSize     = span.sizeInElements();
        T*           segmentData = segment.data();
        if (segment.header.capacityBytes < span.sizeInBytes())
        {
            if (segmentData != nullptr and not segment.isInline())
            {
                VTable::destruct(segment.toSpan());
                Internal::releaseMemory(segment.header, segmentData);
            }
            T* newData = Internal::allocate(segment, span.sizeInBytes());
            segment.setData(newData);
            if (newData == nullptr)
            {
                return false;
            }
            constructFunc({newData, newSize}, span.data());
        }
        else
        {
            const size_t oldSize = segment.size();
            const size_t minSize = min(oldSize, newSize);
            assignFunc({segmentData, minSize}, span.data());
            if (newSize > minSize)
            {
                constructFunc({segmentData + minSize, newSize - minSize}, span.data() + minSize);
            }
            else if (oldSize > minSize)
            {
                VTable::destruct({segmentData + minSize, oldSize - minSize});
            }
        }
        segment.header.sizeBytes = static_cast<uint32_t>(newSize * sizeof(T));
        return true;
    }

    static Span<T> toSpanOffsetElements(Segment& segment, size_t offsetElements)
    {
        return {segment.data() + offsetElements, segment.size() - offsetElements};
    }
};

// clang-format off
template <typename VTable> SC::Segment<VTable>::Segment() noexcept {}
template <typename VTable> SC::Segment<VTable>::~Segment() noexcept { Internal::releaseInternal(*this); }
template <typename VTable> SC::Segment<VTable>::Segment(Segment&& other) noexcept { SC_ASSERT_RELEASE(assignMove(move(other))); }
template <typename VTable> SC::Segment<VTable>::Segment(const Segment& other) noexcept { SC_ASSERT_RELEASE(assign(other.toSpanConst())); }
template <typename VTable> SC::Segment<VTable>& SC::Segment<VTable>::operator=(Segment&& other) noexcept { SC_ASSERT_RELEASE(assignMove(move(other))); return *this;}
template <typename VTable> SC::Segment<VTable>& SC::Segment<VTable>::operator=(const Segment& other) noexcept { SC_ASSERT_RELEASE(assign(other.toSpanConst())); return *this; }
// clang-format on

template <typename VTable>
SC::Segment<VTable>::Segment(uint32_t capacityInBytes)
{
    VTable::header = {capacityInBytes};
    if (capacityInBytes > 0)
    {
        VTable::setData(VTable::getInlineData());
    }
}

template <typename VTable>
bool SC::Segment<VTable>::shrink_to_fit() noexcept
{
    // 1. Can't shrink inline or empty buffers
    if (VTable::header.capacityBytes == 0 or VTable::isInline())
    {
        return true;
    }

    // 2. Rollback to inline segment if it's available and if there is enough capacity
    if (VTable::header.hasInlineData)
    {
        uint32_t inlineCapacity = static_cast<uint32_t>(VTable::getInlineCapacity());
        if (VTable::header.sizeBytes <= inlineCapacity)
        {
            T*      inlineData = VTable::getInlineData();
            Span<T> selfSpan   = toSpan();
            VTable::template moveConstruct<T>(selfSpan, inlineData);
            VTable::destruct(selfSpan);
            Internal::releaseMemory(VTable::header, selfSpan.data());
            VTable::setData(inlineData);
            VTable::header.capacityBytes = inlineCapacity;
            return true; // No need to go on the heap allocation branch
        }
    }

    // 3. Otherwise we are on heap, possibly followed by an insufficient inline segment
    if (VTable::header.sizeBytes < VTable::header.capacityBytes)
    {
        T* newData = Internal::reallocate(*this, VTable::header.sizeBytes);
        VTable::setData(newData);
        if (VTable::header.sizeBytes > 0 and newData == nullptr)
        {
            return false;
        }
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::resize(size_t newSize, const T& value) noexcept
{
    const size_t oldSize = size();
    if (not reserve(newSize))
    {
        return false;
    }
    VTable::header.sizeBytes = static_cast<uint32_t>(newSize * sizeof(T));
    if (newSize > oldSize)
    {
        VTable::template copyConstructAs<T>({data() + oldSize, newSize - oldSize}, value);
    }
    else if (newSize < oldSize)
    {
        VTable::destruct({data() + newSize, oldSize - newSize});
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::resizeWithoutInitializing(size_t newSize) noexcept
{
    if (reserve(newSize))
    {
        VTable::header.sizeBytes = static_cast<uint32_t>(newSize * sizeof(T));
        return true;
    }
    return false;
}

template <typename VTable>
template <typename U>
[[nodiscard]] bool SC::Segment<VTable>::append(Span<const U> span) noexcept
{
    const auto oldSize = size();
    if (resizeWithoutInitializing(oldSize + span.sizeInElements()))
    {
        if (not span.empty())
        {
            VTable::template copyConstruct<U>({data() + oldSize, span.sizeInElements()}, span.data());
        }
        return true;
    }
    return false;
}

template <typename VTable>
template <typename VTable2>
bool SC::Segment<VTable>::appendMove(Segment<VTable2>&& other) noexcept
{
    using U            = typename VTable2::Type;
    const auto oldSize = size();
    if (resizeWithoutInitializing(oldSize + other.size()))
    {
        VTable::template moveConstruct<U>({data() + oldSize, other.size()}, other.data());
        return true;
    }
    return false;
}

template <typename VTable>
bool SC::Segment<VTable>::reserve(size_t capacity) noexcept
{
    const size_t capacityBytes = capacity * sizeof(T);
    if (capacityBytes > SegmentHeader::MaxCapacity)
    {
        return false;
    }
    if (capacityBytes <= VTable::header.capacityBytes)
    {
        return true;
    }

    const bool wasInline = VTable::isInline();
    const bool mustAlloc = VTable::header.capacityBytes == 0 or wasInline;
    T* newData = mustAlloc ? Internal::allocate(*this, capacityBytes) : Internal::reallocate(*this, capacityBytes);
    VTable::setData(newData);
    if (newData == nullptr)
    {
        return false;
    }
    if (wasInline and VTable::header.sizeBytes > 0)
    {
        VTable::template moveConstruct<T>({newData, size()}, VTable::getInlineData());
    }
    return true;
}

template <typename VTable>
void SC::Segment<VTable>::clear() noexcept
{
    VTable::destruct(toSpan());
    VTable::header.sizeBytes = 0;
}

template <typename VTable>
template <typename VTable2>
bool SC::Segment<VTable>::assignMove(Segment<VTable2>&& other) noexcept
{
    using U           = typename VTable2::Type;
    Span<T> selfSpan  = toSpan();
    Span<U> otherSpan = other.toSpan();
    if (selfSpan.data() == otherSpan.data())
    {
        return true;
    }

    if (other.isEmpty())
    {
        Internal::releaseInternal(*this);
        return true;
    }

    if (other.isInline())
    {
        // we cannot steal segment but only copy it (move-assign)
        if (not Internal::assignInternal(&VTable::template moveConstruct<U>, &VTable::template moveAssign<U>, *this,
                                         otherSpan))
        {
            return false;
        }
        VTable::destruct(otherSpan);
        other.header.sizeBytes = 0;
    }
    else
    {
        // Cool we can just steal the heap allocated segment header pointer
        // If other was followed by inline segment we restore its link
        // If other was just heap allocated we set it to nullptr
        if (not VTable::isInline())
        {
            VTable::destruct(selfSpan);
            Internal::releaseMemory(VTable::header, selfSpan.data());
        }
        VTable::setData(otherSpan.data());
        VTable::header.sizeBytes     = other.header.sizeBytes;
        VTable::header.capacityBytes = other.header.capacityBytes;
        other.header.sizeBytes       = 0;
        Internal::eventuallyRestoreInlineData(other);
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::push_back(T&& value) noexcept
{
    const auto numElements = size();
    if (resizeWithoutInitializing(numElements + 1))
    {
        placementNew(data()[numElements], move(value));
        return true;
    }
    return false;
}

template <typename VTable>
bool SC::Segment<VTable>::pop_back(T* removedValue) noexcept
{
    if (isEmpty())
    {
        return false;
    }
    if (removedValue)
    {
        *removedValue = move(back());
    }
    VTable::destruct(Internal::toSpanOffsetElements(*this, size() - 1));
    VTable::header.sizeBytes -= sizeof(T);
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::pop_front(T* removedValue) noexcept
{
    if (isEmpty())
    {
        return false;
    }
    if (removedValue)
    {
        *removedValue = move(front());
    }
    return removeAt(0);
}

template <typename VTable>
template <typename U>
bool SC::Segment<VTable>::assign(Span<const U> span) noexcept
{
    if (span.data() == data())
    {
        return true;
    }
    if (span.empty())
    {
        Internal::releaseInternal(*this);
        return true;
    }
    return Internal::assignInternal(&VTable::template copyConstruct<U>, &VTable::template copyAssign<U>, *this, span);
}

template <typename VTable>
bool SC::Segment<VTable>::removeRange(size_t start, size_t length) noexcept
{
    const size_t numElements = size();
    if (start >= numElements or (start + length) > numElements)
    {
        return false;
    }
    VTable::remove(Internal::toSpanOffsetElements(*this, start), length);
    VTable::header.sizeBytes -= static_cast<uint32_t>(length * sizeof(T));
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::insert(size_t index, Span<const T> data) noexcept
{
    const size_t     numElements = size();
    const size_t     dataSize    = data.sizeInElements();
    constexpr size_t MaxElements = SegmentHeader::MaxCapacity / sizeof(T);
    if ((index > numElements) or (dataSize >= MaxElements - numElements) or not reserve(numElements + dataSize))
    {
        return false;
    }
    if (not data.empty())
    {
        VTable::template copyInsert<T>(Internal::toSpanOffsetElements(*this, index), data);
        VTable::header.sizeBytes += static_cast<uint32_t>(data.sizeInBytes());
    }
    return true;
}
