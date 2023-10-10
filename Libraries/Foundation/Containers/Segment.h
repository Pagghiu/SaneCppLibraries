// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Algorithms/AlgorithmFind.h"
#include "../Algorithms/AlgorithmRemove.h"
#include "../Base/Assert.h"
#include "../Base/InitializerList.h"
#include "../Base/Limits.h"
#include "../Base/Types.h"
#include "../Language/MetaProgramming.h" // EnableIf, IsTriviallyCopyable
#include "../Language/Span.h"

namespace SC
{
template <typename SizeT = SC::uint32_t>
struct SegmentHeaderBase;
template <typename T>
struct SegmentItems;
template <typename Allocator, typename T>
struct SegmentOperations;
} // namespace SC

template <typename SizeT>
struct alignas(SC::uint64_t) SC::SegmentHeaderBase
{
    using SizeType = SizeT;

    static constexpr SizeType MaxValue = (~static_cast<SizeType>(0)) >> 1;

    // Options (isSmallVector etc.) are booleans but declaring them as actual bool makes MSVC add padding bytes
    SizeType sizeBytes : sizeof(SizeType) * 8 - 1;
    SizeType isSmallVector : 1;
    SizeType capacityBytes : sizeof(SizeType) * 8 - 1;
    SizeType isFollowedBySmallVector : 1;

    void initDefaults()
    {
        static_assert(alignof(SegmentHeaderBase) == alignof(uint64_t), "SegmentHeaderBase check alignment");
        static_assert(sizeof(SegmentHeaderBase) == sizeof(SizeType) * 2, "SegmentHeaderBase check alignment");
        isSmallVector           = false;
        isFollowedBySmallVector = false;
    }

    [[nodiscard]] static SegmentHeaderBase* getSegmentHeader(void* oldItems)
    {
        return reinterpret_cast<SegmentHeaderBase*>(static_cast<uint8_t*>(oldItems) - sizeof(SegmentHeaderBase));
    }

    template <typename T>
    [[nodiscard]] T* getItems()
    {
        return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(this) + sizeof(SegmentHeaderBase));
    }
};

namespace SC
{
using SegmentHeader = SegmentHeaderBase<uint32_t>;
}

template <typename T>
struct SC::SegmentItems : public SegmentHeader
{
    // Members

    SegmentItems() { initDefaults(); }

    void setSize(size_t newSize) { sizeBytes = static_cast<SizeType>(newSize * sizeof(T)); }

    [[nodiscard]] size_t size() const { return static_cast<size_t>(sizeBytes / sizeof(T)); }
    [[nodiscard]] size_t capacity() const { return static_cast<size_t>(capacityBytes / sizeof(T)); }

    [[nodiscard]] bool isEmpty() const { return sizeBytes == 0; }

    // Statics

    [[nodiscard]] static SegmentItems* getSegment(T* oldItems)
    {
        return reinterpret_cast<SegmentItems*>(reinterpret_cast<uint8_t*>(oldItems) - sizeof(SegmentHeader));
    }

    [[nodiscard]] static const SegmentItems* getSegment(const T* oldItems)
    {
        return reinterpret_cast<const SegmentItems*>(reinterpret_cast<const uint8_t*>(oldItems) -
                                                     sizeof(SegmentHeader));
    }

    // TODO: needs specialized moveAssign for trivial types
    static void moveAssign(T* destination, size_t indexStart, size_t numElements, T* source);

    static void copyAssign(T* destination, size_t indexStart, size_t numElements, const T* source);

    static void destruct(T* destination, size_t indexStart, size_t numElements);

    static void defaultConstruct(T* destination, size_t indexStart, size_t numElements);

    static void copyConstructSingle(T* destination, size_t indexStart, size_t numElements, const T& sourceValue);

    static void copyConstructMultiple(T* destination, size_t indexStart, size_t numElements, const T* sourceValues);

    static void moveConstruct(T* destination, size_t indexStart, size_t numElements, T* source);

    template <typename U, bool IsCopy>
    static typename EnableIf<IsCopy, void>::type constructItems(T* destination, size_t indexStart, size_t numElements,
                                                                U* sourceValue);

    template <typename U, bool IsCopy>
    static typename EnableIf<not IsCopy, void>::type constructItems(U* destination, size_t indexStart,
                                                                    size_t numElements, U* sourceValue);

    template <typename Q = T>
    static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
    moveItems(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN);

    template <typename Q = T>
    static typename EnableIf<IsTriviallyCopyable<Q>::value, void>::type //
    moveItems(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN);

    template <typename U, typename Q = T>
    static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
    copyItems(T* oldItems, const size_t numToAssign, const size_t numToCopyConstruct, const size_t numToDestroy,
              U* other, size_t otherSize);

    template <typename U, typename Q = T>
    static typename EnableIf<IsTriviallyCopyable<Q>::value, void>::type //
    copyItems(T* oldItems, const size_t numToAssign, const size_t numToCopyConstruct, const size_t numToDestroy,
              U* other, size_t otherSize);

    template <bool IsCopy, typename U, typename Q = T>
    static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
    insertItems(T*& oldItems, size_t position, const size_t numElements, const size_t newSize, U* other,
                size_t otherSize);

    template <bool IsCopy, typename U, typename Q = T>
    static typename EnableIf<IsTriviallyCopyable<U>::value, void>::type //
    insertItems(T*& oldItems, size_t position, const size_t numElements, const size_t newSize, U* other,
                size_t otherSize);

    template <typename Lambda>
    [[nodiscard]] static bool findIf(const T* items, size_t indexStart, const size_t numElements, Lambda&& criteria,
                                     size_t* foundIndex = nullptr);

    template <typename Lambda>
    [[nodiscard]] static bool removeAll(T* items, size_t indexStart, const size_t numElements, Lambda&& criteria);
};

template <typename Allocator, typename T>
struct SC::SegmentOperations
{
    [[nodiscard]] static bool push_back(T*& oldItems, const T& element);

    [[nodiscard]] static bool push_back(T*& oldItems, T&& element);

    template <bool sizeOfTIsSmallerThanInt>
    static typename EnableIf<sizeOfTIsSmallerThanInt == true, void>::type //
    // sizeof(T) <= sizeof(int) (uses memset)
    reserveInternal(T* items, const size_t oldSize, const size_t newSize, const T& defaultValue);
    template <bool sizeOfTIsSmallerThanInt>
    static typename EnableIf<sizeOfTIsSmallerThanInt == false, void>::type //
    // sizeof(T) > sizeof(int) (cannot use memset)
    reserveInternal(T* items, const size_t oldSize, const size_t newSize, const T& defaultValue);

    static void reserveInitialize(T* items, const size_t oldSize, const size_t newSize, const T& defaultValue);

    [[nodiscard]] static bool reallocate(T*& oldItems, size_t newSize);

    template <typename U>
    [[nodiscard]] static bool assign(T*& oldItems, U* other, size_t otherSize);

    template <bool IsCopy, typename U>
    [[nodiscard]] static bool insert(T*& oldItems, size_t position, U* other, size_t otherSize);

    [[nodiscard]] static bool ensureCapacity(T*& oldItems, size_t newCapacity, const size_t keepFirstN);

    template <bool initialize, typename Q = T>
    [[nodiscard]] static typename EnableIf<IsTriviallyCopyable<Q>::value, bool>::type //
    resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue);

    template <bool initialize, typename Q = T>
    [[nodiscard]] static typename EnableIf<not IsTriviallyCopyable<Q>::value, bool>::type //
    resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue);

    [[nodiscard]] static bool shrink_to_fit(T*& oldItems);

    static void clear(SegmentItems<T>* segment);

    static void destroy(SegmentItems<T>* segment);

    [[nodiscard]] static bool pop_back(T* items);

    [[nodiscard]] static bool pop_front(T* items);

    [[nodiscard]] static bool removeAt(T* items, size_t index);
};

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------
// SegmentItems<T>
//-----------------------------------------------------------------------------------------------------------------------

template <typename T>
void SC::SegmentItems<T>::moveAssign(T* destination, size_t indexStart, size_t numElements, T* source)
{
    // TODO: needs specialized moveAssign for trivial types
    // TODO: Should we also call destructor on moved elements?
    for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        destination[idx] = move(source[idx - indexStart]);
}

template <typename T>
void SC::SegmentItems<T>::copyAssign(T* destination, size_t indexStart, size_t numElements, const T* source)
{
    for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        destination[idx] = source[idx - indexStart];
}

template <typename T>
void SC::SegmentItems<T>::destruct(T* destination, size_t indexStart, size_t numElements)
{
    for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        destination[idx].~T();
}

template <typename T>
void SC::SegmentItems<T>::defaultConstruct(T* destination, size_t indexStart, size_t numElements)
{
    for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        new (&destination[idx], PlacementNew()) T;
}

template <typename T>
void SC::SegmentItems<T>::copyConstructSingle(T* destination, size_t indexStart, size_t numElements,
                                              const T& sourceValue)
{
    for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        new (&destination[idx], PlacementNew()) T(sourceValue);
}

template <typename T>
void SC::SegmentItems<T>::copyConstructMultiple(T* destination, size_t indexStart, size_t numElements,
                                                const T* sourceValues)
{
    for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        new (&destination[idx], PlacementNew()) T(sourceValues[idx - indexStart]);
}

template <typename T>
void SC::SegmentItems<T>::moveConstruct(T* destination, size_t indexStart, size_t numElements, T* source)
{
    // TODO: Should we also call destructor on moved elements?
    for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        new (&destination[idx], PlacementNew()) T(move(source[idx - indexStart]));
}

template <typename T>
template <typename U, bool IsCopy>
typename SC::EnableIf<IsCopy, void>::type //
SC::SegmentItems<T>::constructItems(T* destination, size_t indexStart, size_t numElements, U* sourceValue)
{
    copyConstructMultiple(destination, indexStart, numElements, sourceValue);
}

template <typename T>
template <typename U, bool IsCopy>
typename SC::EnableIf<not IsCopy, void>::type //
SC::SegmentItems<T>::constructItems(U* destination, size_t indexStart, size_t numElements, U* sourceValue)
{
    moveConstruct(destination, indexStart, numElements, sourceValue);
}

template <typename T>
template <typename Q>
typename SC::EnableIf<not SC::IsTriviallyCopyable<Q>::value, void>::type //
SC::SegmentItems<T>::moveItems(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN)
{
    moveConstruct(newItems, 0, keepFirstN, oldItems);
    destruct(oldItems, keepFirstN, oldSize - keepFirstN);
}

template <typename T>
template <typename Q>
typename SC::EnableIf<SC::IsTriviallyCopyable<Q>::value, void>::type //
SC::SegmentItems<T>::moveItems(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN)
{
    SC_COMPILER_UNUSED(oldSize);
    // TODO: add code to handle memcpy destination overlapping source
    memcpy(newItems, oldItems, keepFirstN * sizeof(T));
}

template <typename T>
template <typename U, typename Q>
typename SC::EnableIf<not SC::IsTriviallyCopyable<Q>::value, void>::type //
SC::SegmentItems<T>::copyItems(T* oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                               const size_t numToDestroy, U* other, size_t otherSize)
{
    SC_COMPILER_UNUSED(otherSize);
    copyAssign(oldItems, 0, numToAssign, other);
    copyConstructMultiple(oldItems, numToAssign, numToCopyConstruct, other + numToAssign);
    destruct(oldItems, numToAssign + numToCopyConstruct, numToDestroy);
}

template <typename T>
template <typename U, typename Q>
typename SC::EnableIf<SC::IsTriviallyCopyable<Q>::value, void>::type //
SC::SegmentItems<T>::copyItems(T* oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                               const size_t numToDestroy, U* other, size_t otherSize)
{
    SC_COMPILER_UNUSED(numToAssign);
    SC_COMPILER_UNUSED(numToCopyConstruct);
    SC_COMPILER_UNUSED(numToDestroy);
    // TODO: add code to handle memcpy destination overlapping source
    memcpy(oldItems, other, otherSize * sizeof(T));
}

template <typename T>
template <bool IsCopy, typename U, typename Q>
typename SC::EnableIf<not SC::IsTriviallyCopyable<Q>::value, void>::type //
SC::SegmentItems<T>::insertItems(T*& oldItems, size_t position, const size_t numElements, const size_t newSize,
                                 U* other, size_t otherSize)
{
    const size_t numElementsToMove = numElements - position;
    // TODO: not sure if everything works with source elements coming from same buffer as dest
    moveConstruct(oldItems, newSize - numElementsToMove, numElementsToMove, oldItems + position);
    constructItems<U, IsCopy>(oldItems, position, otherSize, other);
}

template <typename T>
template <bool IsCopy, typename U, typename Q>
typename SC::EnableIf<SC::IsTriviallyCopyable<U>::value, void>::type //
SC::SegmentItems<T>::insertItems(T*& oldItems, size_t position, const size_t numElements, const size_t newSize,
                                 U* other, size_t otherSize)
{
    SC_COMPILER_UNUSED(numElements);
    SC_COMPILER_UNUSED(newSize);
    static_assert(sizeof(T) == sizeof(U), "What?");
    // TODO: add code to handle memcpy destination overlapping source
    const size_t numElementsToMove = numElements - position;
    memmove(oldItems + position + otherSize, oldItems + position, numElementsToMove * sizeof(T));
    memcpy(oldItems + position, other, otherSize * sizeof(T));
}

template <typename T>
template <typename Lambda>
bool SC::SegmentItems<T>::findIf(const T* items, size_t indexStart, const size_t numElements, Lambda&& criteria,
                                 size_t* foundIndex)
{
    auto end = items + numElements;
    auto it  = SC::find_if(items + indexStart, end, forward<Lambda>(criteria));
    if (foundIndex)
    {
        *foundIndex = static_cast<size_t>(it - items);
    }
    return it != end;
}

//-----------------------------------------------------------------------------------------------------------------------
// SegmentOperations
//-----------------------------------------------------------------------------------------------------------------------

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::push_back(T*& oldItems, const T& element)
{
    const bool       isNull      = oldItems == nullptr;
    SegmentItems<T>* selfSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
    const size_t     numElements = isNull ? 0 : selfSegment->size();
    const size_t     numCapacity = isNull ? 0 : selfSegment->capacity();
    if (numElements == numCapacity)
    {
        if (!ensureCapacity(oldItems, numElements + 1, numElements))
            SC_LANGUAGE_UNLIKELY { return false; }
    }
    SegmentItems<T>::copyConstructMultiple(oldItems, numElements, 1, &element);
    SegmentItems<T>::getSegment(oldItems)->sizeBytes += sizeof(T);
    return true;
}

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::push_back(T*& oldItems, T&& element)
{
    const bool       isNull      = oldItems == nullptr;
    SegmentItems<T>* selfSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
    const size_t     numElements = isNull ? 0 : selfSegment->size();
    const size_t     numCapacity = isNull ? 0 : selfSegment->capacity();
    if (numElements == numCapacity)
    {
        if (!ensureCapacity(oldItems, numElements + 1, numElements))
            SC_LANGUAGE_UNLIKELY { return false; }
    }
    SegmentItems<T>::moveConstruct(oldItems, numElements, 1, &element);
    SegmentItems<T>::getSegment(oldItems)->sizeBytes += sizeof(T);
    return true;
}

template <typename Allocator, typename T>
template <bool sizeOfTIsSmallerThanInt>
typename SC::EnableIf<sizeOfTIsSmallerThanInt == true, void>::type // sizeof(T) <= sizeof(int)
SC::SegmentOperations<Allocator, T>::reserveInternal(T* items, const size_t oldSize, const size_t newSize,
                                                     const T& defaultValue)
{
    int32_t val = 0;
    memcpy(&val, &defaultValue, sizeof(T));

    // This optimization would need to be split into another template
    // if(sizeof(T) == 1)
    // {
    //     memset(items + oldSize, val, newSize - oldSize);
    // } else

    if (val == 0) // sizeof(T) > sizeof(uint8_t)
    {
        memset(items + oldSize, 0, sizeof(T) * (newSize - oldSize));
    }
    else
    {
        reserveInternal<false>(items, oldSize, newSize, defaultValue);
    }
}

template <typename Allocator, typename T>
template <bool sizeOfTIsSmallerThanInt>
typename SC::EnableIf<sizeOfTIsSmallerThanInt == false, void>::type // sizeof(T) > sizeof(int) (cannot use memset)
SC::SegmentOperations<Allocator, T>::reserveInternal(T* items, const size_t oldSize, const size_t newSize,
                                                     const T& defaultValue)
{
    // This should by copyAssign, but for trivial objects it's the same as copyConstructMultiple
    SegmentItems<T>::copyConstructSingle(items, oldSize, newSize - oldSize, defaultValue);
}

template <typename Allocator, typename T>
void SC::SegmentOperations<Allocator, T>::reserveInitialize(T* items, const size_t oldSize, const size_t newSize,
                                                            const T& defaultValue)
{
    if (newSize > oldSize)
    {
        constexpr bool smallerThanInt = sizeof(T) <= sizeof(int32_t);
        reserveInternal<smallerThanInt>(items, oldSize, newSize, defaultValue);
    }
}

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::reallocate(T*& oldItems, size_t newSize)
{
    SegmentItems<T>* newSegment = nullptr;
    constexpr size_t maxSizeT   = SC::MaxValue();
    if (oldItems == nullptr)
    {
        if (newSize <= maxSizeT / sizeof(T))
        {
            newSegment = static_cast<SegmentItems<T>*>(Allocator::allocate(nullptr, newSize * sizeof(T), &oldItems));
        }
    }
    else if (newSize > SegmentItems<T>::getSegment(oldItems)->capacity())
    {
        if (newSize <= maxSizeT / sizeof(T))
        {
            newSegment = static_cast<SegmentItems<T>*>(
                Allocator::reallocate(SegmentItems<T>::getSegment(oldItems), newSize * sizeof(T)));
        }
    }
    else
    {
        newSegment = SegmentItems<T>::getSegment(oldItems);
    }

    if (newSegment == nullptr)
    {
        return false;
    }
    oldItems = Allocator::template getItems<T>(newSegment);
    return true;
}

template <typename Allocator, typename T>
template <typename U>
bool SC::SegmentOperations<Allocator, T>::assign(T*& oldItems, U* other, size_t otherSize)
{
    const bool       isNull      = oldItems == nullptr;
    SegmentItems<T>* selfSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
    const size_t     oldCapacity = isNull ? 0 : selfSegment->capacity();

    if (otherSize > 0 && otherSize <= oldCapacity)
    {
        const size_t numElements        = isNull ? 0 : selfSegment->size();
        const size_t numToAssign        = min(numElements, otherSize);
        const size_t numToCopyConstruct = otherSize > numElements ? otherSize - numElements : 0;
        const size_t numToDestroy       = numElements > otherSize ? numElements - otherSize : 0;
        SegmentItems<T>::copyItems(oldItems, numToAssign, numToCopyConstruct, numToDestroy, other, otherSize);
        SegmentItems<T>::getSegment(oldItems)->setSize(otherSize);
        return true;
    }
    else
    {
        // otherSize == 0 || otherSize > capacity()
        if (selfSegment != nullptr)
        {
            clear(selfSegment);
        }
        const bool res = insert<true>(oldItems, 0, other, otherSize); // append(other);
        return res;
    }
}

template <typename Allocator, typename T>
template <bool IsCopy, typename U>
bool SC::SegmentOperations<Allocator, T>::insert(T*& oldItems, size_t position, U* other, size_t otherSize)
{
    const bool       isNull      = oldItems == nullptr;
    SegmentItems<T>* selfSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
    const size_t     numElements = isNull ? 0 : selfSegment->size();
    if (position > numElements)
    {
        return false;
    }
    if (otherSize == 0)
    {
        return true;
    }
    constexpr size_t maxSizeT = SC::MaxValue();
    if ((numElements > maxSizeT - otherSize) or ((numElements + otherSize) > maxSizeT / sizeof(T)))
    {
        return false;
    }
    const size_t newSize     = numElements + otherSize;
    const size_t oldCapacity = isNull ? 0 : selfSegment->capacity();
    if (newSize > oldCapacity)
    {
        if (!ensureCapacity(oldItems, newSize, numElements))
        {
            return false;
        }
    }
    // Segment may have been reallocated
    selfSegment = SegmentItems<T>::getSegment(oldItems);
    SegmentItems<T>::template insertItems<IsCopy, U, T>(oldItems, position, numElements, newSize, other, otherSize);
    selfSegment->setSize(newSize);
    return true;
}

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::ensureCapacity(T*& oldItems, size_t newCapacity, const size_t keepFirstN)
{
    const bool       isNull     = oldItems == nullptr;
    SegmentItems<T>* oldSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
    const auto       oldSize    = isNull ? 0 : oldSegment->size();
    SC_ASSERT_DEBUG(oldSize >= keepFirstN);
    SegmentItems<T>* newSegment = nullptr;
    constexpr size_t maxSizeT   = SC::MaxValue();
    if (newCapacity <= maxSizeT / sizeof(T))
    {
        newSegment = static_cast<SegmentItems<T>*>(Allocator::allocate(oldSegment, newCapacity * sizeof(T), &oldItems));
    }
    if (newSegment == oldSegment)
    {
        return false; // Array returning the same as old
    }
    else if (newSegment == nullptr)
    {
        return false;
    }
    newSegment->setSize(oldSize);
    if (oldSize > 0)
    {
        oldSegment = SegmentItems<T>::getSegment(oldItems);
        SegmentItems<T>::moveItems(Allocator::template getItems<T>(oldSegment),
                                   Allocator::template getItems<T>(newSegment), oldSize, keepFirstN);
        Allocator::release(oldSegment);
    }
    oldItems = Allocator::template getItems<T>(newSegment);
    return true;
}

template <typename Allocator, typename T>
template <bool initialize, typename Q>
typename SC::EnableIf<SC::IsTriviallyCopyable<Q>::value, bool>::type //
SC::SegmentOperations<Allocator, T>::resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue)
{
    const auto oldSize = oldItems == nullptr ? 0 : SegmentItems<T>::getSegment(oldItems)->size();

    if (!reallocate(oldItems, newSize))
    {
        return false;
    }

    SegmentItems<T>* selfSegment = SegmentItems<T>::getSegment(oldItems);
    selfSegment->setSize(newSize);
    if (initialize)
    {
        reserveInitialize(Allocator::template getItems<T>(selfSegment), oldSize, newSize, *defaultValue);
    }
    return true;
}

template <typename Allocator, typename T>
template <bool initialize, typename Q>
typename SC::EnableIf<not SC::IsTriviallyCopyable<Q>::value, bool>::type //
SC::SegmentOperations<Allocator, T>::resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue)
{
    static_assert(initialize, "There is no logical reason to skip initializing non trivially copyable class on resize");
    const bool       isNull     = oldItems == nullptr;
    SegmentItems<T>* oldSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
    if (newSize == 0)
    {
        if (oldSegment != nullptr)
        {
            clear(oldSegment);
        }
        return true;
    }
    const auto oldSize     = isNull ? 0 : oldSegment->size();
    const auto oldCapacity = isNull ? 0 : oldSegment->capacity();
    if (newSize > oldCapacity)
    {
        const auto keepFirstN = min(oldSize, newSize);
        if (!ensureCapacity(oldItems, newSize, keepFirstN))
            SC_LANGUAGE_UNLIKELY { return false; }
        if (initialize)
        {
            SegmentItems<T>::copyConstructSingle(oldItems, keepFirstN, newSize - keepFirstN, *defaultValue);
        }
    }
    else
    {
        if (initialize)
        {
            if (oldSize > newSize)
            {
                SegmentItems<T>::destruct(oldItems, newSize, oldSize - newSize);
            }
            else if (oldSize < newSize)
            {
                SegmentItems<T>::copyConstructSingle(oldItems, oldSize, newSize - oldSize, *defaultValue);
            }
        }
    }
    const auto numNewBytes                           = newSize * sizeof(T);
    SegmentItems<T>::getSegment(oldItems)->sizeBytes = static_cast<SegmentHeader::SizeType>(numNewBytes);
    return true;
}

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::shrink_to_fit(T*& oldItems)
{
    const bool       isNull      = oldItems == nullptr;
    SegmentItems<T>* oldSegment  = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
    const size_t     numElements = isNull ? 0 : oldSegment->size();
    if (numElements > 0)
    {
        const size_t selfCapacity = oldSegment->capacity();
        if (numElements != selfCapacity)
        {
            auto newSegment =
                static_cast<SegmentItems<T>*>(Allocator::allocate(oldSegment, oldSegment->sizeBytes, &oldItems));
            if (newSegment == oldSegment)
            {
                return true; // Array allocator returning the same memory
            }
            else if (newSegment == nullptr)
                SC_LANGUAGE_UNLIKELY { return false; }
            newSegment->sizeBytes = oldSegment->sizeBytes;
            SegmentItems<T>::moveConstruct(Allocator::template getItems<T>(newSegment), 0, numElements,
                                           Allocator::template getItems<T>(oldSegment));
            Allocator::release(oldSegment);
            oldItems = Allocator::template getItems<T>(newSegment);
        }
    }
    else
    {
        if (oldSegment != nullptr)
        {
            destroy(oldSegment);
            oldItems = nullptr;
        }
    }
    return true;
}

template <typename Allocator, typename T>
void SC::SegmentOperations<Allocator, T>::clear(SegmentItems<T>* segment)
{
    SegmentItems<T>::destruct(segment->template getItems<T>(), 0, segment->size());
    segment->sizeBytes = 0;
}

template <typename Allocator, typename T>
void SC::SegmentOperations<Allocator, T>::destroy(SegmentItems<T>* segment)
{
    clear(segment);
    Allocator::release(segment);
}

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::pop_back(T* items)
{
    if (items == nullptr)
    {
        return false;
    }
    SegmentItems<T>& segment     = *SegmentItems<T>::getSegment(items);
    const size_t     numElements = segment.size();
    if (numElements == 0)
    {
        return false;
    }
    SegmentItems<T>::destruct(items, numElements - 1, 1);
    segment.setSize(numElements - 1);
    return true;
}

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::pop_front(T* items)
{
    return removeAt(items, 0);
}

template <typename Allocator, typename T>
bool SC::SegmentOperations<Allocator, T>::removeAt(T* items, size_t index)
{
    if (items == nullptr)
    {
        return false;
    }
    auto&        segment     = *SegmentItems<T>::getSegment(items);
    const size_t numElements = segment.size();
    if (index >= numElements or numElements == 0)
    {
        return false;
    }
    SegmentItems<T>::moveAssign(items, index, numElements - index - 1, items + index + 1);
    SegmentItems<T>::destruct(items, numElements - 1, 1);
    segment.setSize(numElements - 1);
    return true;
}

template <typename T>
template <typename Lambda>
bool SC::SegmentItems<T>::removeAll(T* items, size_t indexStart, const size_t numElements, Lambda&& criteria)
{
    if (items == nullptr)
    {
        return false;
    }
    auto& segment = *SegmentItems<T>::getSegment(items);
    auto  end     = items + numElements;
    auto  it      = SC::remove_if(items + indexStart, end, forward<Lambda>(criteria));
    SegmentItems<T>::destruct(it, 0, static_cast<size_t>(end - it));
    segment.setSize(static_cast<size_t>(it - items));
    return it != end;
}
