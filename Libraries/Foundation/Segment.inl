// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "../Foundation/Memory.h"

template <typename VTable>
struct SC::Segment<VTable>::Internal
{
    static SegmentHeader* getInlineHeader(Segment<VTable>& segment)
    {
        return reinterpret_cast<SegmentHeader*>(reinterpret_cast<char*>(&segment.header) + alignof(SegmentHeader));
    }

    static SegmentHeader* allocateNewHeader(size_t newCapacityInBytes, bool isFollowedByInline)
    {
        SegmentHeader* newHeader = VTable::allocateNewHeader(newCapacityInBytes);
        if (newHeader)
        {
            newHeader->sizeBytes                = 0;
            newHeader->capacityBytes            = static_cast<uint32_t>(newCapacityInBytes);
            newHeader->isFollowedByInlineBuffer = isFollowedByInline;
            newHeader->isInlineBuffer           = false;
        }
        return newHeader;
    }

    static SegmentHeader* reallocateExistingHeader(SegmentHeader& src, size_t newCapacityInBytes)
    {
        SegmentHeader* dest = VTable::reallocateExistingHeader(src, newCapacityInBytes);
        if (dest)
        {
            dest->capacityBytes  = static_cast<uint32_t>(newCapacityInBytes);
            dest->isInlineBuffer = false;
        }
        return dest;
    }

    static void releaseInternal(Segment& segment)
    {
        SegmentHeader*& header = segment.header;
        if (header)
        {
            VTable::destruct(*header, 0, header->sizeBytes);
            header->sizeBytes              = 0;
            const bool restoreInlineBuffer = header->isFollowedByInlineBuffer;
            if (not segment.isInlineBuffer())
            {
                VTable::destroyHeader(*header);
                header = restoreInlineBuffer ? Internal::getInlineHeader(segment) : nullptr;
            }
        }
    }

    template <typename Construct, typename Assign, typename U>
    static bool assignInternal(Construct constructFunc, Assign assignFunc, Segment& segment, Span<U> span)
    {
        SegmentHeader*& header    = segment.header;
        const size_t    sizeBytes = header ? header->sizeBytes : 0;
        if (header == nullptr or header->capacityBytes < span.sizeInBytes())
        {
            const bool followed = header != nullptr and (header->isInlineBuffer or header->isFollowedByInlineBuffer);
            if (header != nullptr and not segment.isInlineBuffer())
            {
                VTable::destruct(*header, 0, header->sizeBytes);
                VTable::destroyHeader(*header);
            }
            header = allocateNewHeader(span.sizeInBytes(), followed);
            if (header == nullptr)
            {
                return false;
            }
            header->sizeBytes = static_cast<uint32_t>(span.sizeInBytes());
            constructFunc(*header, 0, span.data(), span.sizeInBytes());
        }
        else
        {
            segment.header->sizeBytes = static_cast<uint32_t>(span.sizeInBytes());
            const size_t assignBytes  = min(sizeBytes, span.sizeInBytes());
            assignFunc(*header, 0, span.data(), assignBytes);
            if (header->sizeBytes > assignBytes)
            {
                constructFunc(*header, assignBytes, span.data() + assignBytes / sizeof(T),
                              header->sizeBytes - assignBytes);
            }
            else if (sizeBytes > assignBytes)
            {
                VTable::destruct(*header, assignBytes, sizeBytes - assignBytes);
            }
        }
        return true;
    }
};

template <typename VTable>
SC::Segment<VTable>::Segment() : header(nullptr)
{}

template <typename VTable>
SC::Segment<VTable>::~Segment()
{
    Internal::releaseInternal(*this);
}

template <typename VTable>
SC::Segment<VTable>::Segment(SegmentHeader& inlineHeader, uint32_t capacityInBytes)
{
    unsafeSetHeader(&inlineHeader);
    inlineHeader.sizeBytes     = 0;
    inlineHeader.capacityBytes = capacityInBytes;

    inlineHeader.isInlineBuffer           = true;
    inlineHeader.isFollowedByInlineBuffer = false;
}

template <typename VTable>
SC::Segment<VTable>::Segment(Segment&& other) : header(nullptr)
{
    SC_ASSERT_RELEASE(assignMove(move(other)));
}

template <typename VTable>
SC::Segment<VTable>& SC::Segment<VTable>::operator=(Segment&& other)
{
    SC_ASSERT_RELEASE(assignMove(move(other)));
    return *this;
}

template <typename VTable>
SC::Segment<VTable>::Segment(const Segment& other) : header(nullptr)
{
    SC_ASSERT_RELEASE(assign(other.toSpanConst()));
}

template <typename VTable>
SC::Segment<VTable>& SC::Segment<VTable>::operator=(const Segment& other)
{
    SC_ASSERT_RELEASE(assign(other.toSpanConst()));
    return *this;
}

template <typename VTable>
bool SC::Segment<VTable>::shrink_to_fit()
{
    // 1. Can't shrink inline or empty buffers
    if (header == nullptr or isInlineBuffer())
    {
        return true;
    }

    // 2. Rollback to inline segment if it's available and if there is enough capacity
    if (header->isFollowedByInlineBuffer)
    {
        SegmentHeader* inlineHeader = Internal::getInlineHeader(*this);
        if (header->sizeBytes <= inlineHeader->capacityBytes)
        {
            VTable::moveConstruct(*inlineHeader, 0, header->getData<T>(), header->sizeBytes);
            inlineHeader->sizeBytes = header->sizeBytes;
            VTable::destruct(*header, 0, header->sizeBytes);
            VTable::destroyHeader(*header);
            header = inlineHeader;
            return true; // No need to go on the heap allocation branch
        }
    }

    // 3. Otherwise we are on heap, possibly followed by an insufficient inline segment
    if (header->sizeBytes < header->capacityBytes)
    {
        SegmentHeader* newHeader = Internal::reallocateExistingHeader(*header, header->sizeBytes);
        if (newHeader == nullptr)
        {
            return false;
        }
        header = newHeader;
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
    header->sizeBytes         = static_cast<uint32_t>(newSizeBytes);
    if (newSizeBytes > previousSizeBytes)
    {
        VTable::copyConstructSingle(*header, previousSizeBytes, &value, newSizeBytes - previousSizeBytes,
                                    sizeof(value));
    }
    else if (newSizeBytes < previousSizeBytes)
    {
        VTable::destruct(*header, newSizeBytes, previousSizeBytes - newSizeBytes);
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::resizeWithoutInitializing(size_t newSize)
{
    if (reserve(newSize))
    {
        header->sizeBytes = static_cast<uint32_t>(newSize * sizeof(T));
        return true;
    }
    return false;
}

template <typename VTable>
bool SC::Segment<VTable>::append(Span<const T> span)
{
    const auto oldSize = header ? header->sizeBytes / sizeof(T) : 0;
    if (resizeWithoutInitializing(oldSize + span.sizeInElements()))
    {
        if (not span.empty())
        {
            VTable::copyConstruct(*header, oldSize * sizeof(T), span.data(), span.sizeInBytes());
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
    const auto oldSize = header ? header->sizeBytes / sizeof(T) : 0;
    if (resizeWithoutInitializing(oldSize + other.size()))
    {
        VTable::moveConstruct(*header, oldSize * sizeof(T), other.data(), other.size() * sizeof(T));
        return true;
    }
    return false;
}

template <typename VTable>
bool SC::Segment<VTable>::reserve(size_t newCapacity)
{
    size_t newCapacityBytes = newCapacity * sizeof(T);
    if (newCapacityBytes > SegmentHeader::MaxCapacity)
    {
        return false;
    }
    if (header != nullptr and newCapacityBytes <= header->capacityBytes)
    {
        return true;
    }
    SegmentHeader* newHeader;

    const bool isInline = isInlineBuffer();
    if (isInline or header == nullptr)
    {
        newHeader = Internal::allocateNewHeader(newCapacityBytes, isInline);
    }
    else
    {
        newHeader = Internal::reallocateExistingHeader(*header, newCapacityBytes);
    }
    if (newHeader == nullptr)
    {
        return false;
    }
    if (isInline)
    {
        VTable::moveConstruct(*newHeader, 0, header->getData<T>(), header->sizeBytes);
        newHeader->sizeBytes = header->sizeBytes;
        header->sizeBytes    = 0;
    }
    header = newHeader;
    return true;
}

template <typename VTable>
void SC::Segment<VTable>::clear()
{
    if (header)
    {
        VTable::destruct(*header, 0, header->sizeBytes);
        header->sizeBytes = 0;
    }
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
        Span<T> span = {other.data(), other.header->sizeBytes / sizeof(T)};
        if (not Internal::assignInternal(&VTable::moveConstruct, &VTable::moveAssign, *this, span))
        {
            return false;
        }
        VTable::destruct(*other.header, 0, other.header->sizeBytes);
        other.header->sizeBytes = 0;
    }
    else
    {
        // Cool we can just steal the heap allocated segment header pointer
        // If other was followed by inline segment we restore its link
        // If other was just heap allocated we set it to nullptr
        const bool followedByInline = isInlineBuffer() or (header != nullptr and header->isFollowedByInlineBuffer);

        if (header != nullptr and not isInlineBuffer())
        {
            VTable::destruct(*header, 0, header->sizeBytes);
            VTable::destroyHeader(*header);
        }
        header = other.header;
        if (other.header->isFollowedByInlineBuffer)
        {
            other.header = Internal::getInlineHeader(other);
        }
        else
        {
            other.header = nullptr;
        }
        header->isFollowedByInlineBuffer = followedByInline;
    }
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::push_back(T&& value)
{
    if (resizeWithoutInitializing(size() + 1))
    {
#if SC_COMPILER_GCC
        // "error: writing 1 byte into a region of size 0 [-Werror=stringop-overflow=]"
        VTable::moveConstruct(*header, (size() - 1) * sizeof(T), &value, sizeof(T));
#else
        placementNew(back(), move(value));
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
    VTable::destruct(*header, header->sizeBytes - sizeof(T), sizeof(T));
    header->sizeBytes -= sizeof(T);
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
    VTable::remove(*header, start * sizeof(T), (start + length) * sizeof(T));
    header->sizeBytes -= static_cast<uint32_t>(length * sizeof(T));
    return true;
}

template <typename VTable>
bool SC::Segment<VTable>::insert(size_t index, Span<const T> data)
{
    const size_t numElements = size();
    const size_t dataSize    = data.sizeInElements();
    if ((index > numElements) or (dataSize >= SegmentHeader::MaxCapacity - numElements) or
        not reserve(numElements + dataSize))
    {
        return false;
    }
    if (not data.empty())
    {
        VTable::copyInsert(*header, index * sizeof(T), data.data(), data.sizeInBytes());
        header->sizeBytes += static_cast<uint32_t>(data.sizeInBytes());
    }
    return true;
}
