#pragma once
#include "Assert.h"
#include "Language.h"
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

    uint32_t sizeBytes;
    uint32_t capacityBytes;
};

template <typename T>
struct SC::SegmentItems : public SegmentHeader
{
    union
    {
        T items[0];
    };
    SegmentItems() {}
    ~SegmentItems() {}

    SegmentItems(const SegmentItems&)            = delete;
    SegmentItems(SegmentItems&&)                 = delete;
    SegmentItems& operator=(const SegmentItems&) = delete;
    SegmentItems& operator=(SegmentItems&&)      = delete;

    [[nodiscard]] size_t size() const { return sizeBytes / sizeof(T); }
    [[nodiscard]] bool   isEmpty() const { return sizeBytes == 0; }
    [[nodiscard]] size_t capacity() const { return capacityBytes / sizeof(T); }

    [[nodiscard]] T*       begin() { return items; }
    [[nodiscard]] const T* begin() const { return items; }
    [[nodiscard]] T*       end() { return items + size(); }
    [[nodiscard]] const T* end() const { return items + size(); }
    [[nodiscard]] T*       data() { return items; }
    [[nodiscard]] const T* data() const { return items; }

    [[nodiscard]] T& front()
    {
        const size_t numElements = size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[0];
    }

    [[nodiscard]] const T& front() const
    {
        const size_t numElements = size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[0];
    }

    [[nodiscard]] T& back()
    {
        const size_t numElements = size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[numElements - 1];
    }

    [[nodiscard]] const T& back() const
    {
        const size_t numElements = size();
        SC_RELEASE_ASSERT(numElements > 0);
        return items[numElements - 1];
    }
    [[nodiscard]] static SegmentItems<T>* getSegment(T* oldItems)
    {
        return reinterpret_cast<SegmentItems<T>*>(reinterpret_cast<uint8_t*>(oldItems) - sizeof(SegmentHeader));
    }

    [[nodiscard]] static const SegmentItems<T>* getSegment(const T* oldItems)
    {
        return reinterpret_cast<SegmentItems<T>*>(reinterpret_cast<uint8_t*>(oldItems) - sizeof(SegmentHeader));
    }

    void setSize(size_t newSize) { sizeBytes = static_cast<HeaderBytesType>(newSize * sizeof(T)); }

    [[nodiscard]] bool pop_back()
    {
        const size_t numElements = size();
        if (numElements > 0)
        {
            SegmentItems<T>::destroyElements(items, numElements - 1, 1);
            setSize(numElements - 1);
            return true;
        }
        else
        {
            return false;
        }
    }

    [[nodiscard]] bool pop_front()
    {
        const size_t numElements = size();
        if (numElements > 0)
        {
            SegmentItems<T>::moveAssignElements(items, 0, numElements - 1, items + 1);
            setSize(numElements - 1);
            return true;
        }
        else
        {
            return false;
        }
    }

    void clear()
    {
        SegmentItems<T>::destroyElements(items, 0, size());
        setSize(0);
    }

    template <typename Comparison = SmallerThan<T>>
    void sort(Comparison comparison = Comparison())
    {
        bubbleSort(begin(), end(), comparison);
    }

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
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(newItems, oldItems, keepFirstN * sizeof(T));
    }

    template <typename U, typename Q = T>
    static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
    copyReplaceTrivialOrNot(T*& oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                            const size_t numToDestroy, U* other, size_t otherSize)
    {
        copyAssignElements(oldItems, 0, numToAssign, other);
        copyConstruct(oldItems, numToAssign, numToCopyConstruct, other + numToAssign);
        destroyElements(oldItems, numToAssign + numToCopyConstruct, numToDestroy);
    }

    template <typename U, typename Q = T>
    static typename EnableIf<IsTriviallyCopyable<Q>::value, void>::type //
    copyReplaceTrivialOrNot(T*& oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                            const size_t numToDestroy, U* other, size_t otherSize)
    {
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
        static_assert(sizeof(T) == sizeof(U), "What?");
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(oldItems + position, other, otherSize * sizeof(T));
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
            if (!ensureCapacity(oldItems, numElements + 1, numElements)) [[unlikely]]
            {
                return false;
            }
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
            if (!ensureCapacity(oldItems, numElements + 1, numElements)) [[unlikely]]
            {
                return false;
            }
        }
        SegmentItems<T>::moveConstruct(oldItems, numElements, 1, &element);
        SegmentItems<T>::getSegment(oldItems)->sizeBytes += sizeof(T);
        return true;
    }

    static void reserveInternalTrivialInitialize(T* items, const size_t oldSize, const size_t newSize,
                                                 const T& defaultValue)
    {
        if (newSize > oldSize)
        {
            int32_t    val            = 0;
            const bool smallerThanInt = sizeof(T) <= sizeof(int32_t);
            if (smallerThanInt)
            {
                memcpy(&val, &defaultValue, sizeof(T));
            }
            if (smallerThanInt && val == 0)
            {
                memset(items + oldSize, 0, sizeof(T) * (newSize - oldSize));
            }
            else
            {
                // This should by copyAssign, but for trivial objects it's the same as copyConstruct
                SegmentItems<T>::copyConstructSingle(items, oldSize, newSize - oldSize, defaultValue);
            }
        }
    }

    [[nodiscard]] static bool reserveInternalTrivialAllocate(T*& oldItems, size_t newSize)
    {
        SegmentItems<T>* newSegment;
        if (oldItems == nullptr)
        {
            newSegment = static_cast<SegmentItems<T>*>(Allocator::allocate(nullptr, newSize * sizeof(T)));
        }
        else if (newSize > SegmentItems<T>::getSegment(oldItems)->capacity())
        {
            newSegment = static_cast<SegmentItems<T>*>(
                Allocator::reallocate(SegmentItems<T>::getSegment(oldItems), newSize * sizeof(T)));
        }
        else
        {
            newSegment = SegmentItems<T>::getSegment(oldItems);
        }

        if (newSegment == nullptr)
        {
            return false;
        }
        oldItems = newSegment->items;
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
                selfSegment->clear();
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
        const bool       isNull      = oldItems == nullptr;
        SegmentItems<T>* oldSegment  = isNull ? nullptr : SegmentItems<T>::getSegment(oldItems);
        const auto       oldSize     = isNull ? 0 : oldSegment->size();
        const auto       numNewBytes = newCapacity * sizeof(T);
        SC_DEBUG_ASSERT(oldSize >= keepFirstN);
        auto newSegment = static_cast<SegmentItems<T>*>(Allocator::allocate(oldSegment, numNewBytes));
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
            auto oldSegment = SegmentItems<T>::getSegment(oldItems);
            SegmentItems<T>::moveAndDestroy(oldSegment->items, newSegment->items, oldSize, keepFirstN);
            Allocator::release(oldSegment);
        }
        oldItems = newSegment->items;
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
            reserveInternalTrivialInitialize(selfSegment->items, oldSize, newSize, *defaultValue);
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
                oldSegment->clear();
            }
            return true;
        }
        const auto oldSize     = isNull ? 0 : oldSegment->size();
        const auto oldCapacity = isNull ? 0 : oldSegment->capacity();
        if (newSize > oldCapacity)
        {
            const auto keepFirstN = min(oldSize, newSize);
            if (!ensureCapacity(oldItems, newSize, keepFirstN)) [[unlikely]]
            {
                return false;
            }
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
                SegmentItems<T>* newSegment =
                    static_cast<SegmentItems<T>*>(Allocator::allocate(oldSegment, oldSegment->sizeBytes));
                if (newSegment == oldSegment)
                {
                    return true; // Array allocator returning the same memory
                }
                else if (newSegment == nullptr) [[unlikely]]
                {
                    return false;
                }
                newSegment->sizeBytes = oldSegment->sizeBytes;
                SegmentItems<T>::moveConstruct(newSegment->items, 0, numElements, oldSegment->items);
                Allocator::release(oldSegment);
                oldItems = newSegment->items;
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

    static void destroy(SegmentItems<T>* newSegment)
    {
        newSegment->clear();
        Allocator::release(newSegment);
    }
};

template <typename Allocator, typename T, int N>
struct alignas(SC::uint64_t) SC::Segment : public SegmentItems<T>
{
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

    ~Segment() { operations::destroy(this); }

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
            T*         items = Segment::items;
            const bool res   = Segment::operations::copy(items, other.items, other.size());
            (void)res;
            SC_DEBUG_ASSERT(res);
        }
        return *this;
    }

    Segment& operator=(Segment&& other)
    {
        if (&other != this)
        {
            SegmentItems<T>::getSegment(items)->clear();
            if (appendMove(other.items, other.size()))
            {
                other.clear();
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
};
