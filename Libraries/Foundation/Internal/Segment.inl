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

    static void* allocate(Segment& segment, size_t capacityBytes, bool restoreInlineBuffer)
    {
        void* newData = Internal::allocateMemory(segment.header, capacityBytes);
        if (newData)
        {
            segment.header.sizeBytes           = 0;
            segment.header.capacityBytes       = static_cast<uint32_t>(capacityBytes);
            segment.header.restoreInlineBuffer = restoreInlineBuffer;
        }
        return newData;
    }

    static void* reallocate(Segment& segment, size_t capacityBytes)
    {
        Span<T> data    = segment.toSpan();
        void*   newData = nullptr;
        if SC_LANGUAGE_IF_CONSTEXPR (not VTable::IsArray)
        {
            if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            {
                newData = Internal::reallocateMemory(segment.header, data.data(), capacityBytes);
            }
            else
            {
                // TODO: Room for optimization for memcpy-able objects (a >= subset than trivially copyable)
                newData = Internal::allocateMemory(segment.header, capacityBytes);
                if (newData != nullptr)
                {
                    VTable::moveConstruct({reinterpret_cast<T*>(newData), data.sizeInElements()}, data.data());
                    VTable::destruct(data);
                    Internal::releaseMemory(segment.header, data.data());
                }
            }
        }
        segment.header.capacityBytes = static_cast<uint32_t>(capacityBytes);
        return newData;
    }

    static void releaseInternal(Segment& segment)
    {
        VTable::destruct(segment.toSpan());
        segment.header.sizeBytes       = 0;
        const bool restoreInlineBuffer = segment.header.restoreInlineBuffer;
        if (not segment.isInlineBuffer())
        {
            Internal::releaseMemory(segment.header, segment.getData());
            if (restoreInlineBuffer)
            {
                segment.setData(segment.getInlineData());
                segment.header.capacityBytes       = segment.getInlineCapacity();
                segment.header.restoreInlineBuffer = false;
            }
            else
            {
                segment.setData(nullptr);
                segment.header.capacityBytes = 0;
            }
        }
    }

    template <typename Construct, typename Assign, typename U>
    static bool assignInternal(Construct constructFunc, Assign assignFunc, Segment& segment, Span<U> span)
    {
        SegmentHeader& header        = segment.header;
        const size_t   prevSizeBytes = header.sizeBytes;
        if (segment.getData() == nullptr or header.capacityBytes < span.sizeInBytes())
        {
            const bool restoreInlineBuffer = (segment.isInlineBuffer() or header.restoreInlineBuffer);
            if (not segment.isInlineBuffer())
            {
                VTable::destruct(segment.toSpan());
                Internal::releaseMemory(segment.header, segment.getData());
            }
            void* newData = Internal::allocate(segment, span.sizeInBytes(), restoreInlineBuffer);
            segment.setData(newData);
            if (newData == nullptr)
            {
                return false;
            }
            header.sizeBytes = static_cast<uint32_t>(span.sizeInBytes());
            constructFunc({segment.data(), span.sizeInElements()}, span.data());
        }
        else
        {
            segment.header.sizeBytes = static_cast<uint32_t>(span.sizeInBytes());
            const size_t assignBytes = min(prevSizeBytes, span.sizeInBytes());
            assignFunc({segment.data(), assignBytes / sizeof(T)}, span.data());
            if (header.sizeBytes > assignBytes)
            {
                constructFunc({segment.data() + assignBytes / sizeof(T), (header.sizeBytes - assignBytes) / sizeof(T)},
                              span.data() + assignBytes / sizeof(T));
            }
            else if (prevSizeBytes > assignBytes)
            {
                VTable::destruct({segment.data() + assignBytes / sizeof(T), (prevSizeBytes - assignBytes) / sizeof(T)});
            }
        }
        return true;
    }

    static Span<T> toSpanOffsetElements(Segment& segment, size_t offsetElements)
    {
        return {segment.data() + offsetElements, segment.header.sizeBytes / sizeof(T) - offsetElements};
    }
};

// clang-format off
template <> inline SC::detail::SegmentData<false>::SegmentData(uint32_t capacity) : header(capacity) {}
inline SC::detail::SegmentData<true>::SegmentData(uint32_t capacity) : header(capacity) {}

template <typename VTable> SC::Segment<VTable>::Segment() {}
template <typename VTable> SC::Segment<VTable>::Segment(Segment&& other) { SC_ASSERT_RELEASE(assignMove(move(other))); }
template <typename VTable> SC::Segment<VTable>::Segment(const Segment& other) { SC_ASSERT_RELEASE(assign(other.toSpanConst())); }

template <typename VTable> SC::Segment<VTable>::~Segment() { Internal::releaseInternal(*this); }

template <typename VTable> SC::Segment<VTable>& SC::Segment<VTable>::operator=(Segment&& other) { SC_ASSERT_RELEASE(assignMove(move(other))); return *this; }
template <typename VTable> SC::Segment<VTable>& SC::Segment<VTable>::operator=(const Segment& other) { SC_ASSERT_RELEASE(assign(other.toSpanConst())); return *this; }
// clang-format on

template <typename VTable>
SC::Segment<VTable>::Segment(uint32_t capacityInBytes) : Parent(capacityInBytes)
{
    if (capacityInBytes > 0)
    {
        Parent::setData(Parent::getInlineData());
    }
}

template <typename VTable>
bool SC::Segment<VTable>::shrink_to_fit()
{
    // 1. Can't shrink inline or empty buffers
    if (Parent::getData() == nullptr or isInlineBuffer())
    {
        return true;
    }

    // 2. Rollback to inline segment if it's available and if there is enough capacity
    if (header.restoreInlineBuffer)
    {
        uint32_t inlineCapacity = Parent::getInlineCapacity();
        if (header.sizeBytes <= inlineCapacity)
        {
            T* inlineData = reinterpret_cast<T*>(Parent::getInlineData());
            VTable::moveConstruct(toSpan(), inlineData);
            VTable::destruct(toSpan());
            Internal::releaseMemory(header, Parent::getData());
            Parent::setData(inlineData);
            header.capacityBytes       = inlineCapacity;
            header.restoreInlineBuffer = false;
            return true; // No need to go on the heap allocation branch
        }
    }

    // 3. Otherwise we are on heap, possibly followed by an insufficient inline segment
    if (header.sizeBytes < header.capacityBytes)
    {
        void* newData = Internal::reallocate(*this, header.sizeBytes);
        Parent::setData(newData);
        if (header.sizeBytes > 0 and newData == nullptr)
        {
            return false;
        }
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::resize(size_t newSize, const T& value)
{
    const size_t previousSizeBytes = size() * sizeof(T);
    if (not reserve(newSize))
    {
        return false;
    }
    const size_t newSizeBytes = newSize * sizeof(T);
    header.sizeBytes          = static_cast<uint32_t>(newSizeBytes);
    if (newSizeBytes > previousSizeBytes)
    {
        Span<T> newElements = {data() + previousSizeBytes / sizeof(T), (newSizeBytes - previousSizeBytes) / sizeof(T)};
        VTable::copyConstructAs(newElements, Span<const T>(value));
    }
    else if (newSizeBytes < previousSizeBytes)
    {
        VTable::destruct({data() + newSizeBytes / sizeof(T), (previousSizeBytes - newSizeBytes) / sizeof(T)});
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::resizeWithoutInitializing(size_t newSize)
{
    if (reserve(newSize))
    {
        header.sizeBytes = static_cast<uint32_t>(newSize * sizeof(T));
        return true;
    }
    return false;
}

template <typename VTable>
bool SC::Segment<VTable>::append(Span<const T> span)
{
    const auto oldSize = header.sizeBytes / sizeof(T);
    if (resizeWithoutInitializing(oldSize + span.sizeInElements()))
    {
        if (not span.empty())
        {
            VTable::copyConstruct({data() + oldSize, span.sizeInBytes() / sizeof(T)}, span.data());
        }
        return true;
    }
    return false;
}

template <typename VTable>
template <typename U>
[[nodiscard]] bool SC::Segment<VTable>::append(Span<const U> span)
{
    for (const U& it : span)
    {
        if (not push_back(it))
            return false;
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::appendMove(Segment&& other)
{
    const auto oldSize = header.sizeBytes / sizeof(T);
    if (resizeWithoutInitializing(oldSize + other.size()))
    {
        VTable::moveConstruct({data() + oldSize, other.size()}, other.data());
        return true;
    }
    return false;
}

template <typename VTable>
bool SC::Segment<VTable>::reserve(size_t newCapacity)
{
    const size_t newCapacityBytes = newCapacity * sizeof(T);
    if (newCapacityBytes > SegmentHeader::MaxCapacity)
    {
        return false;
    }
    if (newCapacityBytes <= header.capacityBytes)
    {
        return true;
    }

    const bool     isInline     = isInlineBuffer();
    const uint32_t oldSizeBytes = header.sizeBytes;
    void*          newData;
    if (isInline or Parent::getData() == nullptr)
    {
        newData = Internal::allocate(*this, newCapacityBytes, isInline);
    }
    else
    {
        newData = Internal::reallocate(*this, newCapacityBytes);
    }
    Parent::setData(newData);
    if (newData == nullptr)
    {
        return false;
    }
    if (isInline)
    {
        VTable::moveConstruct({data(), oldSizeBytes / sizeof(T)}, reinterpret_cast<T*>(Parent::getInlineData()));
        header.sizeBytes = oldSizeBytes;
    }
    return true;
}

template <typename VTable>
void SC::Segment<VTable>::clear()
{
    VTable::destruct(toSpan());
    header.sizeBytes = 0;
}

template <typename VTable>
bool SC::Segment<VTable>::assignMove(Segment&& other)
{
    if (&other == this)
    {
        return true;
    }

    if (other.isEmpty())
    {
        Internal::releaseInternal(*this);
        return true;
    }

    if (other.isInlineBuffer())
    {
        // we cannot steal segment but only copy it (move-assign)
        Span<T> span = {other.data(), other.header.sizeBytes / sizeof(T)};
        if (not Internal::assignInternal(&VTable::moveConstruct, &VTable::moveAssign, *this, span))
        {
            return false;
        }
        VTable::destruct(other.toSpan());
        other.header.sizeBytes = 0;
    }
    else
    {
        // Cool we can just steal the heap allocated segment header pointer
        // If other was followed by inline segment we restore its link
        // If other was just heap allocated we set it to nullptr
        const bool restoreInlineBuffer = isInlineBuffer() or (header.restoreInlineBuffer);

        if (not isInlineBuffer())
        {
            VTable::destruct(toSpan());
            Internal::releaseMemory(header, Parent::getData());
        }
        Parent::setData(other.getData());
        if (other.header.restoreInlineBuffer)
        {
            other.setData(other.getInlineData());
            other.header.restoreInlineBuffer = false;
            other.header.capacityBytes       = other.getInlineCapacity();
        }
        else
        {
            other.setData(nullptr);
        }
        header                 = other.header;
        other.header.sizeBytes = 0;

        header.restoreInlineBuffer = restoreInlineBuffer;
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::push_back(T&& value)
{
    const auto currentSize = size();
    if (resizeWithoutInitializing(currentSize + 1))
    {
#if SC_COMPILER_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
#endif
        placementNew(data()[currentSize], move(value));
#if SC_COMPILER_GCC
#pragma GCC diagnostic pop
#endif
        return true;
    }
    return false;
}

template <typename VTable>
bool SC::Segment<VTable>::pop_back(T* removedValue)
{
    if (isEmpty())
    {
        return false;
    }
    if (removedValue)
    {
        *removedValue = move(back());
    }
    VTable::destruct({data() + (header.sizeBytes - sizeof(T)) / sizeof(T), 1});
    header.sizeBytes -= sizeof(T);
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::pop_front(T* removedValue)
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
bool SC::Segment<VTable>::assign(Span<const T> span)
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
    return Internal::assignInternal(&VTable::copyConstruct, &VTable::copyAssign, *this, span);
}

template <typename VTable>
bool SC::Segment<VTable>::removeRange(size_t start, size_t length)
{
    const size_t numElements = size();
    if (start >= numElements or (start + length) > numElements)
    {
        return false;
    }
    VTable::remove(Internal::toSpanOffsetElements(*this, start), length);
    header.sizeBytes -= static_cast<uint32_t>(length * sizeof(T));
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::insert(size_t index, Span<const T> data)
{
    const size_t numElements = size();
    const size_t dataSize    = data.sizeInElements();
    if ((index > numElements) or (dataSize >= SegmentHeader::MaxCapacity / sizeof(T) - numElements) or
        not reserve(numElements + dataSize))
    {
        return false;
    }
    if (not data.empty())
    {
        VTable::copyInsert(Internal::toSpanOffsetElements(*this, index), data);
        header.sizeBytes += static_cast<uint32_t>(data.sizeInBytes());
    }
    return true;
}
