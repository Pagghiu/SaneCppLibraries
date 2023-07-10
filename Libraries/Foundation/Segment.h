// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Assert.h"
#include "InitializerList.h"
#include "Language.h"
#include "Limits.h"
#include "Span.h"
#include "Types.h"

namespace SC
{
struct SegmentHeader;
template <typename T>
struct SegmentItems;
template <typename Allocator, typename T>
struct SegmentOperations;
template <typename Allocator, typename T, int N>
struct Segment;
} // namespace SC

struct SC::SegmentHeader
{
    typedef uint32_t HeaderBytesType;
    struct Options
    {
        bool isSmallVector;
        bool isFollowedBySmallVector;
    };

    alignas(uint64_t) Options options;
    uint32_t sizeBytes;
    uint32_t capacityBytes;

    void initDefaults()
    {
        options.isSmallVector           = false;
        options.isFollowedBySmallVector = false;
    }

    [[nodiscard]] static SegmentHeader* getSegmentHeader(void* oldItems)
    {
        return static_cast<SegmentHeader*>(static_cast<void*>(static_cast<uint8_t*>(oldItems) - sizeof(SegmentHeader)));
    }
};

template <typename T>
struct SC::SegmentItems : public SegmentHeader
{
    SegmentItems() { initDefaults(); }
    [[nodiscard]] size_t size() const { return sizeBytes / sizeof(T); }
    [[nodiscard]] bool   isEmpty() const { return sizeBytes == 0; }
    [[nodiscard]] size_t capacity() const { return capacityBytes / sizeof(T); }

    [[nodiscard]] static SegmentItems* getSegment(T* oldItems)
    {
        return static_cast<SegmentItems*>(
            static_cast<void*>(static_cast<uint8_t*>(static_cast<void*>(oldItems)) - sizeof(SegmentHeader)));
    }

    [[nodiscard]] static const SegmentItems* getSegment(const T* oldItems)
    {
        return static_cast<const SegmentItems*>(static_cast<const void*>(
            static_cast<const uint8_t*>(static_cast<const void*>(oldItems)) - sizeof(SegmentHeader)));
    }

    void setSize(size_t newSize) { sizeBytes = static_cast<HeaderBytesType>(newSize * sizeof(T)); }

    // TODO: needs specialized moveAssign for trivial types
    static void moveAssignElements(T* destination, size_t indexStart, size_t numElements, T* source)
    {
        // TODO: Should we also call destructor on moved elements?
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            destination[idx] = move(source[idx - indexStart]);
    }

    static void copyAssignElements(T* destination, size_t indexStart, size_t numElements, const T* source)
    {
        // TODO: Should we also call destructor on moved elements?
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            destination[idx] = source[idx - indexStart];
    }

    static void destroyElements(T* destination, size_t indexStart, size_t numElements)
    {
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            destination[idx].~T();
    }

    static void defaultConstruct(T* destination, size_t indexStart, size_t numElements)
    {
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            new (&destination[idx], PlacementNew()) T;
    }

    static void copyConstructSingle(T* destination, size_t indexStart, size_t numElements, const T& sourceValue)
    {
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            new (&destination[idx], PlacementNew()) T(sourceValue);
    }

    static void copyConstruct(T* destination, size_t indexStart, size_t numElements, const T* sourceValues)
    {
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            new (&destination[idx], PlacementNew()) T(sourceValues[idx - indexStart]);
    }

    static void moveConstruct(T* destination, size_t indexStart, size_t numElements, T* source)
    {
        // TODO: Should we also call destructor on moved elements?
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            new (&destination[idx], PlacementNew()) T(move(source[idx - indexStart]));
    }
    template <typename U, bool copy>
    static typename EnableIf<copy, void>::type copyOrMoveConstruct(T* destination, size_t indexStart,
                                                                   size_t numElements, U* sourceValue)
    {
        copyConstruct(destination, indexStart, numElements, sourceValue);
    }

    template <typename U, bool copy>
    static typename EnableIf<not copy, void>::type copyOrMoveConstruct(U* destination, size_t indexStart,
                                                                       size_t numElements, U* sourceValue)
    {
        moveConstruct(destination, indexStart, numElements, sourceValue);
    }

    template <typename Q = T>
    static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
    moveAndDestroy(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN)
    {
        moveConstruct(newItems, 0, keepFirstN, oldItems);
        destroyElements(oldItems, keepFirstN, oldSize - keepFirstN);
    }

    template <typename Q = T>
    static typename EnableIf<IsTriviallyCopyable<Q>::value, void>::type //
    moveAndDestroy(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN)
    {
        SC_UNUSED(oldSize);
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(newItems, oldItems, keepFirstN * sizeof(T));
    }

    template <typename U, typename Q = T>
    static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
    copyReplaceTrivialOrNot(T*& oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                            const size_t numToDestroy, U* other, size_t otherSize)
    {
        SC_UNUSED(otherSize);
        copyAssignElements(oldItems, 0, numToAssign, other);
        copyConstruct(oldItems, numToAssign, numToCopyConstruct, other + numToAssign);
        destroyElements(oldItems, numToAssign + numToCopyConstruct, numToDestroy);
    }

    template <typename U, typename Q = T>
    static typename EnableIf<IsTriviallyCopyable<Q>::value, void>::type //
    copyReplaceTrivialOrNot(T*& oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                            const size_t numToDestroy, U* other, size_t otherSize)
    {
        SC_UNUSED(numToAssign);
        SC_UNUSED(numToCopyConstruct);
        SC_UNUSED(numToDestroy);
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(oldItems, other, otherSize * sizeof(T));
    }

    template <bool copy, typename U, typename Q = T>
    static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
    insertItemsTrivialOrNot(T*& oldItems, size_t position, const size_t numElements, const size_t newSize, U* other,
                            size_t otherSize)
    {
        const size_t numElementsToMove = numElements - position;
        // TODO: not sure if everything works with source elements coming from same buffer as dest
        moveConstruct(oldItems, newSize - numElementsToMove, numElementsToMove, oldItems + position);
        copyOrMoveConstruct<U, copy>(oldItems, position, otherSize, other);
    }

    template <bool copy, typename U, typename Q = T>
    static typename EnableIf<IsTriviallyCopyable<U>::value, void>::type //
    insertItemsTrivialOrNot(T*& oldItems, size_t position, const size_t numElements, const size_t newSize, U* other,
                            size_t otherSize)
    {
        SC_UNUSED(numElements);
        SC_UNUSED(newSize);
        static_assert(sizeof(T) == sizeof(U), "What?");
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(oldItems + position, other, otherSize * sizeof(T));
    }

    template <typename Lambda>
    [[nodiscard]] static bool findIf(const T* items, size_t indexStart, const size_t numElements, Lambda&& criteria,
                                     size_t* foundIndex = nullptr)
    {
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
        {
            if (criteria(items[idx]))
            {
                if (foundIndex)
                {
                    *foundIndex = idx;
                }
                return true;
            }
        }
        return false;
    }
};

template <typename Allocator, typename T>
struct SC::SegmentOperations
{
    [[nodiscard]] static bool push_back(T*& oldItems, const T& element)
    {
        const bool       isNull      = oldItems == nullptr;
        SegmentItems<T>* selfSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
        const size_t     numElements = isNull ? 0 : selfSegment->size();
        const size_t     numCapacity = isNull ? 0 : selfSegment->capacity();
        if (numElements == numCapacity)
        {
            if (!ensureCapacity(oldItems, numElements + 1, numElements))
                SC_UNLIKELY { return false; }
        }
        SegmentItems<T>::copyConstruct(oldItems, numElements, 1, &element);
        SegmentItems<T>::getSegment(oldItems)->sizeBytes += sizeof(T);
        return true;
    }

    [[nodiscard]] static bool push_back(T*& oldItems, T&& element)
    {
        const bool       isNull      = oldItems == nullptr;
        SegmentItems<T>* selfSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
        const size_t     numElements = isNull ? 0 : selfSegment->size();
        const size_t     numCapacity = isNull ? 0 : selfSegment->capacity();
        if (numElements == numCapacity)
        {
            if (!ensureCapacity(oldItems, numElements + 1, numElements))
                SC_UNLIKELY { return false; }
        }
        SegmentItems<T>::moveConstruct(oldItems, numElements, 1, &element);
        SegmentItems<T>::getSegment(oldItems)->sizeBytes += sizeof(T);
        return true;
    }

    template <bool sizeOfTIsSmallerThanInt>
    static typename EnableIf<sizeOfTIsSmallerThanInt == true, void>::type // sizeof(T) <= sizeof(int)
    reserveInternalTrivialInitializeTemplate(T* items, const size_t oldSize, const size_t newSize,
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
            reserveInternalTrivialInitializeTemplate<false>(items, oldSize, newSize, defaultValue);
        }
    }

    template <bool sizeOfTIsSmallerThanInt>
    static
        typename EnableIf<sizeOfTIsSmallerThanInt == false, void>::type // sizeof(T) > sizeof(int) (cannot use memset)
        reserveInternalTrivialInitializeTemplate(T* items, const size_t oldSize, const size_t newSize,
                                                 const T& defaultValue)
    {
        // This should by copyAssign, but for trivial objects it's the same as copyConstruct
        SegmentItems<T>::copyConstructSingle(items, oldSize, newSize - oldSize, defaultValue);
    }

    static void reserveInternalTrivialInitialize(T* items, const size_t oldSize, const size_t newSize,
                                                 const T& defaultValue)
    {
        if (newSize > oldSize)
        {
            constexpr bool smallerThanInt = sizeof(T) <= sizeof(int32_t);
            reserveInternalTrivialInitializeTemplate<smallerThanInt>(items, oldSize, newSize, defaultValue);
        }
    }

    [[nodiscard]] static bool reserveInternalTrivialAllocate(T*& oldItems, size_t newSize)
    {
        SegmentItems<T>* newSegment = nullptr;
        constexpr size_t maxSizeT   = SC::MaxValue();
        if (oldItems == nullptr)
        {
            if (newSize <= maxSizeT / sizeof(T))
            {
                newSegment =
                    static_cast<SegmentItems<T>*>(Allocator::allocate(nullptr, newSize * sizeof(T), &oldItems));
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

    template <typename U>
    [[nodiscard]] static bool copy(T*& oldItems, U* other, size_t otherSize)
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
            SegmentItems<T>::copyReplaceTrivialOrNot(oldItems, numToAssign, numToCopyConstruct, numToDestroy, other,
                                                     otherSize);
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
            const bool res = insert<true>(oldItems, 0, other, otherSize); // appendCopy(other);
            return res;
        }
    }

    template <bool copy, typename U>
    [[nodiscard]] static bool insert(T*& oldItems, size_t position, U* other, size_t otherSize)
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
        SegmentItems<T>::template insertItemsTrivialOrNot<copy, U, T>(oldItems, position, numElements, newSize, other,
                                                                      otherSize);
        selfSegment->setSize(newSize);
        return true;
    }

    [[nodiscard]] static bool ensureCapacity(T*& oldItems, size_t newCapacity, const size_t keepFirstN)
    {
        const bool       isNull     = oldItems == nullptr;
        SegmentItems<T>* oldSegment = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
        const auto       oldSize    = isNull ? 0 : oldSegment->size();
        SC_DEBUG_ASSERT(oldSize >= keepFirstN);
        SegmentItems<T>* newSegment = nullptr;
        constexpr size_t maxSizeT   = SC::MaxValue();
        if (newCapacity <= maxSizeT / sizeof(T))
        {
            newSegment =
                static_cast<SegmentItems<T>*>(Allocator::allocate(oldSegment, newCapacity * sizeof(T), &oldItems));
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
            SegmentItems<T>::moveAndDestroy(Allocator::template getItems<T>(oldSegment),
                                            Allocator::template getItems<T>(newSegment), oldSize, keepFirstN);
            Allocator::release(oldSegment);
        }
        oldItems = Allocator::template getItems<T>(newSegment);
        return true;
    }

    template <bool initialize, typename Q = T>
    [[nodiscard]] static typename EnableIf<IsTriviallyCopyable<Q>::value, bool>::type //
    resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue)
    {
        const auto oldSize = oldItems == nullptr ? 0 : SegmentItems<T>::getSegment(oldItems)->size();

        if (!reserveInternalTrivialAllocate(oldItems, newSize))
        {
            return false;
        }

        SegmentItems<T>* selfSegment = SegmentItems<T>::getSegment(oldItems);
        selfSegment->setSize(newSize);
        if (initialize)
        {
            reserveInternalTrivialInitialize(Allocator::template getItems<T>(selfSegment), oldSize, newSize,
                                             *defaultValue);
        }
        return true;
    }

    template <bool initialize, typename Q = T>
    [[nodiscard]] static typename EnableIf<not IsTriviallyCopyable<Q>::value, bool>::type //
    resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue)
    {
        static_assert(initialize,
                      "There is no logical reason to skip initializing non trivially copyable class on resize");
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
                SC_UNLIKELY { return false; }
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
                    SegmentItems<T>::destroyElements(oldItems, newSize, oldSize - newSize);
                }
                else if (oldSize < newSize)
                {
                    SegmentItems<T>::copyConstructSingle(oldItems, oldSize, newSize - oldSize, *defaultValue);
                }
            }
        }
        const auto numNewBytes                           = newSize * sizeof(T);
        SegmentItems<T>::getSegment(oldItems)->sizeBytes = static_cast<SegmentHeader::HeaderBytesType>(numNewBytes);
        return true;
    }

    [[nodiscard]] static bool shrink_to_fit(T*& oldItems)
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
                    SC_UNLIKELY { return false; }
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

    static void clear(SegmentItems<T>* segment)
    {
        SegmentItems<T>::destroyElements(Allocator::template getItems<T>(segment), 0, segment->size());
        segment->sizeBytes = 0;
    }

    static void destroy(SegmentItems<T>* segment)
    {
        clear(segment);
        Allocator::release(segment);
    }

    [[nodiscard]] static bool pop_back(T*& items)
    {
        const bool       isNull  = items == nullptr;
        SegmentItems<T>* segment = isNull ? nullptr : SegmentItems<T>::getSegment(items);
        if (segment == nullptr)
            return false;
        const size_t numElements = segment->size();
        if (numElements > 0)
        {
            SegmentItems<T>::destroyElements(Allocator::template getItems<T>(segment), numElements - 1, 1);
            segment->setSize(numElements - 1);
            return true;
        }
        else
        {
            return false;
        }
    }

    [[nodiscard]] static bool pop_front(T*& items) { return removeAt(items, 0); }

    [[nodiscard]] static bool removeAt(T*& items, size_t index)
    {
        const bool       isNull  = items == nullptr;
        SegmentItems<T>* segment = isNull ? nullptr : SegmentItems<T>::getSegment(items);
        if (segment == nullptr)
            return false;
        const size_t numElements = segment->size();
        if (index < numElements)
        {
            if (index + 1 == numElements)
            {
                SegmentItems<T>::destroyElements(items, index, 1);
            }
            else
            {
                SegmentItems<T>::moveAssignElements(items, index, numElements - index - 1, items + index + 1);
            }
            segment->setSize(numElements - 1);
            return true;
        }
        else
        {
            return false;
        }
    }
};

template <typename Allocator, typename T, int N>
struct alignas(SC::uint64_t) SC::Segment : public SegmentItems<T>
{
    static_assert(N > 0, "Array must have N > 0");
    typedef SegmentItems<T>                 Parent;
    typedef SegmentOperations<Allocator, T> operations;
    union
    {
        T items[N];
    };

    Segment()
    {
        Segment::sizeBytes     = 0;
        Segment::capacityBytes = sizeof(T) * N;
    }

    Segment(std::initializer_list<T> ilist)
    {
        const auto sz = min(static_cast<int>(ilist.size()), N);
        Parent::copyConstruct(items, 0, static_cast<size_t>(sz), ilist.begin());
        Parent::setSize(static_cast<size_t>(sz));
    }
    ~Segment() { operations::destroy(this); }

    Span<const T> toSpanConst() const { return {items, Parent::sizeBytes}; }
    Span<T>       toSpan() { return {items, Parent::sizeBytes}; }

    [[nodiscard]] T& front()
    {
        const size_t numElements = Parent::size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[0];
    }

    [[nodiscard]] const T& front() const
    {
        const size_t numElements = Parent::size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[0];
    }

    [[nodiscard]] T& back()
    {
        const size_t numElements = Parent::size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[numElements - 1];
    }

    [[nodiscard]] const T& back() const
    {
        const size_t numElements = Parent::size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[numElements - 1];
    }
    Segment(const Segment& other)
    {
        Segment::sizeBytes     = 0;
        Segment::capacityBytes = sizeof(T) * N;
        (void)appendCopy(other.items, other.size());
    }

    Segment(Segment&& other)
    {
        Segment::sizeBytes     = 0;
        Segment::capacityBytes = sizeof(T) * N;
        (void)appendMove(other.items, other.size());
    }

    Segment& operator=(const Segment& other)
    {
        if (&other != this)
        {
            T*         tItems = Segment::items;
            const bool res    = Segment::operations::copy(tItems, other.items, other.size());
            (void)res;
            SC_DEBUG_ASSERT(res);
        }
        return *this;
    }

    Segment& operator=(Segment&& other)
    {
        if (&other != this)
        {
            operations::clear(SegmentItems<T>::getSegment(items));
            if (appendMove(other.items, other.size()))
            {
                operations::clear(SegmentItems<T>::getSegment(other.items));
            }
        }
        return *this;
    }

    template <typename Allocator2, int M>
    Segment(const Segment<Allocator2, T, M>& other)
    {
        static_assert(M <= N, "Unsafe operation, cannot report failure inside constructor, use appendCopy instead");
        Segment::sizeBytes     = 0;
        Segment::capacityBytes = sizeof(T) * N;
        (void)appendCopy(other.items, other.size());
    }
    template <typename Allocator2, int M>
    Segment(Segment<Allocator2, T, M>&& other)
    {
        static_assert(M <= N, "Unsafe operation, cannot report failure inside constructor, use appendMove instead");
        Segment::sizeBytes     = 0;
        Segment::capacityBytes = sizeof(T) * N;
        (void)appendMove(other.items, other.size());
    }

    template <typename Allocator2, int M>
    Segment& operator=(const Segment<Allocator2, T, M>& other)
    {
        if (&other != this)
        {
            T*         items = items;
            const bool res   = copy(items, other.data(), other.size());
            (void)res;
            SC_DEBUG_ASSERT(res);
        }
        return *this;
    }

    template <typename Allocator2, int M>
    Segment& operator=(Segment<Allocator2, T, M>&& other)
    {
        if (&other != this)
        {
            Segment::clear();
            if (appendMove(other.items, other.size()))
            {
                other.clear();
            }
        }
        return *this;
    }

    [[nodiscard]] bool push_back(const T& element)
    {
        T* oldItems = items;
        return operations::push_back(oldItems, element);
    }

    [[nodiscard]] bool push_back(T&& element)
    {
        T* oldItems = items;
        return operations::push_back(oldItems, forward<T>(element));
    }

    [[nodiscard]] bool pop_back()
    {
        T* oldItems = items;
        return operations::pop_back(oldItems);
    }

    [[nodiscard]] bool pop_front()
    {
        T* oldItems = items;
        return operations::pop_front(oldItems);
    }

    [[nodiscard]] T& operator[](size_t index)
    {
        SC_DEBUG_ASSERT(index < Segment::size());
        return items[index];
    }

    [[nodiscard]] const T& operator[](size_t index) const
    {
        SC_DEBUG_ASSERT(index < Segment::size());
        return items[index];
    }

    [[nodiscard]] bool reserve(size_t newCap) { return newCap <= Segment::capacity(); }
    [[nodiscard]] bool resize(size_t newSize, const T& value = T())
    {
        T* oldItems = items;
        return operations::template resizeInternal<true>(oldItems, newSize, &value);
    }
    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        T* oldItems = items;
        return operations::template resizeInternal<false>(oldItems, newSize, nullptr);
    }

    [[nodiscard]] bool shrink_to_fit()
    {
        T* oldItems = items;
        return operations::shrink_to_fit(oldItems);
    }

    [[nodiscard]] T*       begin() { return items; }
    [[nodiscard]] const T* begin() const { return items; }
    [[nodiscard]] T*       end() { return items + Segment::size(); }
    [[nodiscard]] const T* end() const { return items + Segment::size(); }
    [[nodiscard]] T*       data() { return items; }
    [[nodiscard]] const T* data() const { return items; }

    [[nodiscard]] bool insertMove(size_t idx, T* src, size_t srcSize)
    {
        T* oldItems = items;
        return operations::template insert<false>(oldItems, idx, src, srcSize);
    }
    [[nodiscard]] bool insertCopy(size_t idx, const T* src, size_t srcSize)
    {
        T* oldItems = items;
        return operations::template insert<true>(oldItems, idx, src, srcSize);
    }
    [[nodiscard]] bool appendMove(T* src, size_t srcNumItems)
    {
        T* oldItems = items;
        return operations::template insert<false>(oldItems, Segment::size(), src, srcNumItems);
    }
    [[nodiscard]] bool appendCopy(const T* src, size_t srcNumItems)
    {
        T* oldItems = items;
        return operations::template insert<true>(oldItems, Segment::size(), src, srcNumItems);
    }
    template <typename U>
    [[nodiscard]] bool appendMove(U&& src)
    {
        if (appendMove(src.data(), src.size()))
        {
            src.clear();
            return true;
        }
        return false;
    }
    template <typename U>
    [[nodiscard]] bool appendCopy(const U& src)
    {
        return appendCopy(src.data(), src.size());
    }

    [[nodiscard]] bool push_back(std::initializer_list<T> src) { return appendCopy(src.begin(), src.size()); }

    template <typename U>
    [[nodiscard]] bool push_back(Span<U> src)
    {
        const auto oldSize = Segment::size();
        if (reserve(src.sizeInElements()))
        {
            for (auto& it : src)
            {
                if (not push_back(it))
                {
                    break;
                }
            }
        }
        if (oldSize + src.sizeInElements() != Segment::size())
        {
            SC_TRUST_RESULT(resize(oldSize));
            return false;
        }
        return true;
    }

    [[nodiscard]] bool contains(const T& value, size_t* foundIndex = nullptr) const
    {
        T* oldItems = items;
        return SegmentItems<T>::findIf(
            oldItems, 0, Segment::size(), [&](const T& element) { return element == value; }, foundIndex);
    }

    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* foundIndex = nullptr) const
    {
        T* oldItems = items;
        return SegmentItems<T>::findIf(oldItems, 0, Segment::size(), forward<Lambda>(lambda), foundIndex);
    }

    [[nodiscard]] bool removeAt(size_t index)
    {
        T* oldItems = items;
        return SegmentItems<T>::removeAt(oldItems, index);
    }

    template <typename Lambda>
    [[nodiscard]] bool removeAll(Lambda&& criteria)
    {
        size_t index;
        size_t prevIndex         = 0;
        bool   atLeastOneRemoved = false;
        while (SegmentItems<T>::findIf(items, prevIndex, SegmentItems<T>::size() - prevIndex, forward<Lambda>(criteria),
                                       &index))
        {
            SC_TRY_IF(removeAt(index));
            prevIndex         = index;
            atLeastOneRemoved = true;
        }
        return atLeastOneRemoved;
    }

    [[nodiscard]] bool remove(const T& value)
    {
        size_t index;
        if (contains(value, &index))
        {
            return removeAt(index);
        }
        return false;
    }
};
