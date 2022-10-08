#pragma once
#include "assert.h"
#include "language.h"
#include "types.h"

namespace sanecpp
{
struct segmentHeader;
template <typename T>
struct segmentItems;
template <typename Allocator, typename T>
struct segmentOperations;
template <typename Allocator, typename T, int N>
struct segment;
} // namespace sanecpp

struct sanecpp::segmentHeader
{
    typedef uint32_t HeaderBytesType;

    uint32_t sizeBytes;
    uint32_t capacityBytes;
};

template <typename T>
struct sanecpp::segmentItems : public segmentHeader
{
    union
    {
        T items[0];
    };
    segmentItems() {}
    ~segmentItems() {}

    segmentItems(const segmentItems&)            = delete;
    segmentItems(segmentItems&&)                 = delete;
    segmentItems& operator=(const segmentItems&) = delete;
    segmentItems& operator=(segmentItems&&)      = delete;

    [[nodiscard]] size_t size() const { return sizeBytes / sizeof(T); }
    [[nodiscard]] bool   isEmpty() const { return sizeBytes == 0; }
    [[nodiscard]] size_t capacity() const { return capacityBytes / sizeof(T); }

    [[nodiscard]] T& front()
    {
        const size_t numElements = size();
        SANECPP_RELEASE_ASSERT(numElements > 0);
        return items[0];
    }

    [[nodiscard]] const T& front() const
    {
        const size_t numElements = size();
        SANECPP_RELEASE_ASSERT(numElements > 0);
        return items[0];
    }

    [[nodiscard]] T& back()
    {
        const size_t numElements = size();
        SANECPP_RELEASE_ASSERT(numElements > 0);
        return items[numElements - 1];
    }

    [[nodiscard]] const T& back() const
    {
        const size_t numElements = size();
        SANECPP_RELEASE_ASSERT(numElements > 0);
        return items[numElements - 1];
    }
    [[nodiscard]] static segmentItems<T>* getSegment(T* oldItems)
    {
        return reinterpret_cast<segmentItems<T>*>(reinterpret_cast<uint8_t*>(oldItems) - sizeof(segmentHeader));
    }

    [[nodiscard]] static const segmentItems<T>* getSegment(const T* oldItems)
    {
        return reinterpret_cast<segmentItems<T>*>(reinterpret_cast<uint8_t*>(oldItems) - sizeof(segmentHeader));
    }

    void setSize(size_t newSize) { sizeBytes = static_cast<HeaderBytesType>(newSize * sizeof(T)); }

    [[nodiscard]] bool pop_back()
    {
        const size_t numElements = size();
        if (numElements > 0)
        {
            segmentItems<T>::destroyElements(items, numElements - 1, 1);
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
            segmentItems<T>::moveAssignElements(items, 0, numElements - 1, items + 1);
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
        segmentItems<T>::destroyElements(items, 0, size());
        setSize(0);
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
    static typename enable_if<copy, void>::type copyOrMoveConstruct(T* destination, size_t indexStart,
                                                                    size_t numElements, U* sourceValue)
    {
        copyConstruct(destination, indexStart, numElements, sourceValue);
    }

    template <typename U, bool copy>
    static typename enable_if<not copy, void>::type copyOrMoveConstruct(U* destination, size_t indexStart,
                                                                        size_t numElements, U* sourceValue)
    {
        moveConstruct(destination, indexStart, numElements, sourceValue);
    }

    template <typename Q = T>
    static typename enable_if<not is_trivially_copyable<Q>::value, void>::type //
    moveAndDestroy(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN)
    {
        moveConstruct(newItems, 0, keepFirstN, oldItems);
        destroyElements(oldItems, keepFirstN, oldSize - keepFirstN);
    }

    template <typename Q = T>
    static typename enable_if<is_trivially_copyable<Q>::value, void>::type //
    moveAndDestroy(T* oldItems, T* newItems, const size_t oldSize, const size_t keepFirstN)
    {
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(newItems, oldItems, keepFirstN * sizeof(T));
    }

    template <typename U, typename Q = T>
    static typename enable_if<not is_trivially_copyable<Q>::value, void>::type //
    copyReplaceTrivialOrNot(T*& oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                            const size_t numToDestroy, U* other, size_t otherSize)
    {
        copyAssignElements(oldItems, 0, numToAssign, other);
        copyConstruct(oldItems, numToAssign, numToCopyConstruct, other + numToAssign);
        destroyElements(oldItems, numToAssign + numToCopyConstruct, numToDestroy);
    }

    template <typename U, typename Q = T>
    static typename enable_if<is_trivially_copyable<Q>::value, void>::type //
    copyReplaceTrivialOrNot(T*& oldItems, const size_t numToAssign, const size_t numToCopyConstruct,
                            const size_t numToDestroy, U* other, size_t otherSize)
    {
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(oldItems, other, otherSize * sizeof(T));
    }

    template <bool copy, typename U, typename Q = T>
    static typename enable_if<not is_trivially_copyable<Q>::value, void>::type //
    insertItemsTrivialOrNot(T*& oldItems, size_t position, const size_t numElements, const size_t newSize, U* other,
                            size_t otherSize)
    {
        const size_t numElementsToMove = numElements - position;
        // TODO: not sure if everything works with source elements coming from same buffer as dest
        moveConstruct(oldItems, newSize - numElementsToMove, numElementsToMove, oldItems + position);
        copyOrMoveConstruct<U, copy>(oldItems, position, otherSize, other);
    }

    template <bool copy, typename U, typename Q = T>
    static typename enable_if<is_trivially_copyable<U>::value, void>::type //
    insertItemsTrivialOrNot(T*& oldItems, size_t position, const size_t numElements, const size_t newSize, U* other,
                            size_t otherSize)
    {
        static_assert(sizeof(T) == sizeof(U), "What?");
        // TODO: add code to handle memcpy destination overlapping source
        memcpy(oldItems + position, other, otherSize * sizeof(T));
    }
};

template <typename Allocator, typename T>
struct sanecpp::segmentOperations
{
    [[nodiscard]] static bool push_back(T*& oldItems, const T& element)
    {
        const bool       isNull      = oldItems == nullptr;
        segmentItems<T>* selfSegment = isNull ? nullptr : segmentItems<T>::getSegment(oldItems);
        const size_t     numElements = isNull ? 0 : selfSegment->size();
        const size_t     numCapacity = isNull ? 0 : selfSegment->capacity();
        if (numElements == numCapacity)
        {
            if (!ensureCapacity(oldItems, numElements + 1, numElements)) [[unlikely]]
            {
                return false;
            }
        }
        segmentItems<T>::copyConstruct(oldItems, numElements, 1, &element);
        segmentItems<T>::getSegment(oldItems)->sizeBytes += sizeof(T);
        return true;
    }

    [[nodiscard]] static bool push_back(T*& oldItems, T&& element)
    {
        const bool       isNull      = oldItems == nullptr;
        segmentItems<T>* selfSegment = isNull ? nullptr : segmentItems<T>::getSegment(oldItems);
        const size_t     numElements = isNull ? 0 : selfSegment->size();
        const size_t     numCapacity = isNull ? 0 : selfSegment->capacity();
        if (numElements == numCapacity)
        {
            if (!ensureCapacity(oldItems, numElements + 1, numElements)) [[unlikely]]
            {
                return false;
            }
        }
        segmentItems<T>::moveConstruct(oldItems, numElements, 1, &element);
        segmentItems<T>::getSegment(oldItems)->sizeBytes += sizeof(T);
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
                segmentItems<T>::copyConstructSingle(items, oldSize, newSize - oldSize, defaultValue);
            }
        }
    }

    [[nodiscard]] static bool reserveInternalTrivialAllocate(T*& oldItems, size_t newSize)
    {
        segmentItems<T>* newSegment;
        if (oldItems == nullptr)
        {
            newSegment = static_cast<segmentItems<T>*>(Allocator::allocate(nullptr, newSize * sizeof(T)));
        }
        else if (newSize > segmentItems<T>::getSegment(oldItems)->capacity())
        {
            newSegment = static_cast<segmentItems<T>*>(
                Allocator::reallocate(segmentItems<T>::getSegment(oldItems), newSize * sizeof(T)));
        }
        else
        {
            newSegment = segmentItems<T>::getSegment(oldItems);
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
        segmentItems<T>* selfSegment = isNull ? nullptr : segmentItems<T>::getSegment(oldItems);
        const size_t     oldCapacity = isNull ? 0 : selfSegment->capacity();

        if (otherSize > 0 && otherSize <= oldCapacity)
        {
            const size_t numElements        = isNull ? 0 : selfSegment->size();
            const size_t numToAssign        = min(numElements, otherSize);
            const size_t numToCopyConstruct = otherSize > numElements ? otherSize - numElements : 0;
            const size_t numToDestroy       = numElements > otherSize ? numElements - otherSize : 0;
            segmentItems<T>::copyReplaceTrivialOrNot(oldItems, numToAssign, numToCopyConstruct, numToDestroy, other,
                                                     otherSize);
            segmentItems<T>::getSegment(oldItems)->setSize(otherSize);
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
        segmentItems<T>* selfSegment = isNull ? nullptr : segmentItems<T>::getSegment(oldItems);
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
        // segment may have been reallocated
        selfSegment = segmentItems<T>::getSegment(oldItems);
        segmentItems<T>::template insertItemsTrivialOrNot<copy, U, T>(oldItems, position, numElements, newSize, other,
                                                                      otherSize);
        selfSegment->setSize(newSize);
        return true;
    }

    [[nodiscard]] static bool ensureCapacity(T*& oldItems, size_t newCapacity, const size_t keepFirstN)
    {
        const bool       isNull      = oldItems == nullptr;
        segmentItems<T>* oldSegment  = isNull ? nullptr : segmentItems<T>::getSegment(oldItems);
        const auto       oldSize     = isNull ? 0 : oldSegment->size();
        const auto       numNewBytes = newCapacity * sizeof(T);
        SANECPP_DEBUG_ASSERT(oldSize >= keepFirstN);
        auto newSegment = static_cast<segmentItems<T>*>(Allocator::allocate(oldSegment, numNewBytes));
        if (newSegment == oldSegment)
        {
            return false; // array returning the same as old
        }
        else if (newSegment == nullptr)
        {
            return false;
        }
        newSegment->setSize(oldSize);
        if (oldSize > 0)
        {
            auto oldSegment = segmentItems<T>::getSegment(oldItems);
            segmentItems<T>::moveAndDestroy(oldSegment->items, newSegment->items, oldSize, keepFirstN);
            Allocator::release(oldSegment);
        }
        oldItems = newSegment->items;
        return true;
    }

    template <bool initialize, typename Q = T>
    [[nodiscard]] static typename enable_if<is_trivially_copyable<Q>::value, bool>::type //
    resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue)
    {
        const auto oldSize = oldItems == nullptr ? 0 : segmentItems<T>::getSegment(oldItems)->size();

        if (!reserveInternalTrivialAllocate(oldItems, newSize))
        {
            return false;
        }

        segmentItems<T>* selfSegment = segmentItems<T>::getSegment(oldItems);
        selfSegment->setSize(newSize);
        if (initialize)
        {
            reserveInternalTrivialInitialize(selfSegment->items, oldSize, newSize, *defaultValue);
        }
        return true;
    }

    template <bool initialize, typename Q = T>
    [[nodiscard]] static typename enable_if<not is_trivially_copyable<Q>::value, bool>::type //
    resizeInternal(T*& oldItems, size_t newSize, const T* defaultValue)
    {
        static_assert(initialize,
                      "There is no logical reason to skip initializing non trivially copyable class on resize");
        const bool       isNull     = oldItems == nullptr;
        segmentItems<T>* oldSegment = isNull ? nullptr : segmentItems<T>::getSegment(oldItems);
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
                segmentItems<T>::copyConstructSingle(oldItems, keepFirstN, newSize - keepFirstN, *defaultValue);
            }
        }
        else
        {
            if (initialize)
            {

                if (oldSize > newSize)
                {
                    segmentItems<T>::destroyElements(oldItems, newSize, oldSize - newSize);
                }
                else if (oldSize < newSize)
                {
                    segmentItems<T>::copyConstructSingle(oldItems, oldSize, newSize - oldSize, *defaultValue);
                }
            }
        }
        const auto numNewBytes                           = newSize * sizeof(T);
        segmentItems<T>::getSegment(oldItems)->sizeBytes = static_cast<segmentHeader::HeaderBytesType>(numNewBytes);
        return true;
    }

    [[nodiscard]] static bool shrink_to_fit(T*& oldItems)
    {
        const bool       isNull      = oldItems == nullptr;
        segmentItems<T>* oldSegment  = isNull ? nullptr : segmentItems<T>::getSegment(oldItems);
        const size_t     numElements = isNull ? 0 : oldSegment->size();
        if (numElements > 0)
        {
            const size_t selfCapacity = oldSegment->capacity();
            if (numElements != selfCapacity)
            {
                segmentItems<T>* newSegment =
                    static_cast<segmentItems<T>*>(Allocator::allocate(oldSegment, oldSegment->sizeBytes));
                if (newSegment == oldSegment)
                {
                    return true; // array allocator returning the same memory
                }
                else if (newSegment == nullptr) [[unlikely]]
                {
                    return false;
                }
                newSegment->sizeBytes = oldSegment->sizeBytes;
                segmentItems<T>::moveConstruct(newSegment->items, 0, numElements, oldSegment->items);
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

    static void destroy(segmentItems<T>* newSegment)
    {
        newSegment->clear();
        Allocator::release(newSegment);
    }
};

template <typename Allocator, typename T, int N>
struct alignas(sanecpp::uint64_t) sanecpp::segment : public segmentItems<T>
{
    typedef segmentOperations<Allocator, T> operations;
    union
    {
        T items[N];
    };

    segment()
    {
        segment::sizeBytes     = 0;
        segment::capacityBytes = sizeof(T) * N;
    }

    ~segment() { operations::destroy(this); }

    segment(const segment& other)
    {
        segment::sizeBytes     = 0;
        segment::capacityBytes = sizeof(T) * N;
        (void)appendCopy(other.items, other.size());
    }

    segment(segment&& other)
    {
        segment::sizeBytes     = 0;
        segment::capacityBytes = sizeof(T) * N;
        (void)appendMove(other.items, other.size());
    }

    segment& operator=(const segment& other)
    {
        if (&other != this)
        {
            T*         items = segment::items;
            const bool res   = segment::operations::copy(items, other.items, other.size());
            (void)res;
            SANECPP_DEBUG_ASSERT(res);
        }
        return *this;
    }

    segment& operator=(segment&& other)
    {
        if (&other != this)
        {
            segmentItems<T>::getSegment(items)->clear();
            if (appendMove(other.items, other.size()))
            {
                other.clear();
            }
        }
        return *this;
    }

    template <typename Allocator2, int M>
    segment(const segment<Allocator2, T, M>& other)
    {
        static_assert(M <= N, "Unsafe operation, cannot report failure inside constructor, use appendCopy instead");
        segment::sizeBytes     = 0;
        segment::capacityBytes = sizeof(T) * N;
        (void)appendCopy(other.items, other.size());
    }
    template <typename Allocator2, int M>
    segment(segment<Allocator2, T, M>&& other)
    {
        static_assert(M <= N, "Unsafe operation, cannot report failure inside constructor, use appendMove instead");
        segment::sizeBytes     = 0;
        segment::capacityBytes = sizeof(T) * N;
        (void)appendMove(other.items, other.size());
    }

    template <typename Allocator2, int M>
    segment& operator=(const segment<Allocator2, T, M>& other)
    {
        if (&other != this)
        {
            T*         items = items;
            const bool res   = copy(items, other.data(), other.size());
            (void)res;
            SANECPP_DEBUG_ASSERT(res);
        }
        return *this;
    }

    template <typename Allocator2, int M>
    segment& operator=(segment<Allocator2, T, M>&& other)
    {
        if (&other != this)
        {
            segment::clear();
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
        SANECPP_DEBUG_ASSERT(index < segment::size());
        return items[index];
    }

    [[nodiscard]] const T& operator[](size_t index) const
    {
        SANECPP_DEBUG_ASSERT(index < segment::size());
        return items[index];
    }

    [[nodiscard]] bool reserve(size_t newCap) { return newCap <= segment::capacity(); }
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
    [[nodiscard]] T*       end() { return items + segment::size(); }
    [[nodiscard]] const T* end() const { return items + segment::size(); }
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
        return operations::template insert<false>(oldItems, segment::size(), src, srcNumItems);
    }
    [[nodiscard]] bool appendCopy(const T* src, size_t srcNumItems)
    {
        T* oldItems = items;
        return operations::template insert<true>(oldItems, segment::size(), src, srcNumItems);
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
