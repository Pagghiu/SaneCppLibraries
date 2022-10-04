#pragma once
#include "assert.h"
#include "language.h"
#include "limits.h"
#include "memory.h"
#include "types.h"

namespace sanecpp
{
template <typename T, int N = 0>
struct vector;
template <typename T, int N>
struct segmentBase;
template <typename T, int N>
struct segment;
} // namespace sanecpp
template <typename T, int N>
struct alignas(sanecpp::uint64_t) sanecpp::segmentBase
{
    typedef uint32_t HeaderBytesType;

    uint32_t sizeBytes;
    uint32_t capacityBytes;
    union
    {
        T items[N];
    };
};

template <typename T, int N>
struct sanecpp::segment : public sanecpp::segmentBase<T, N>
{
    static segment* reallocate(segment* oldHeader, size_t newSize) { return nullptr; }
    static segment* allocate(size_t numNewBytes) { return nullptr; }
    static void     release(segment* oldHeader) {}
};

template <typename T>
struct sanecpp::segment<T, 0> : public sanecpp::segmentBase<T, 0>
{
    static segment* reallocate(segment* oldHeader, size_t newSize)
    {
        if (newSize > static_cast<typename segmentBase<T, 0>::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        segment* newHeader = reinterpret_cast<segment*>(memoryReallocate(oldHeader, sizeof(segment) + newSize));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<uint32_t>(newSize);
        }
        return newHeader;
    }

    static segment* allocate(size_t numNewBytes)
    {
        if (numNewBytes > static_cast<typename segmentBase<T, 0>::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        segment* newHeader = reinterpret_cast<segment*>(memoryAllocate(sizeof(segment) + numNewBytes));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<uint32_t>(numNewBytes);
        }
        return newHeader;
    }

    static void release(segment* oldHeader) { memoryRelease(oldHeader); }
};

template <typename T, int N>
struct sanecpp::vector
{
    typedef segment<T, N> segment;

    T* items;

    vector() : items(nullptr) { static_assert(sizeof(segment) == 8, "header changed"); }
    vector(vector&& other) : items(other.items) { other.items = nullptr; }
    vector(const vector& other) : items(nullptr) { *this = other; }
    ~vector() { destruct(); }

    vector& operator=(vector&& other)
    {
        if (&other != this)
        {
            destruct();
            items       = other.items;
            other.items = nullptr;
        }
        return *this;
    }

    vector& operator=(const vector& other)
    {
        if (&other != this)
        {
            destruct();
            // We could assign items min(size, other.size) if capacity()>=other.size()...
            if (other.size() > 0)
            {
                segment* newHeader   = segment::allocate(other.getSegment()->sizeBytes);
                newHeader->sizeBytes = newHeader->capacityBytes;
                items                = newHeader->items;
                copyConstruct(items, 0, other.size(), other.items);
            }
        }
        return *this;
    }

    [[nodiscard]] T& operator[](size_t index)
    {
        SANECPP_DEBUG_ASSERT(index < size());
        return items[index];
    }

    [[nodiscard]] const T& operator[](size_t index) const
    {
        SANECPP_DEBUG_ASSERT(index < size());
        return items[index];
    }

    [[nodiscard]] bool push_back(const T& element)
    {
        const size_t numElements = size();
        if (numElements == capacity())
        {
            if (!ensureCapacity(numElements + 1, numElements)) [[unlikely]]
            {
                return false;
            }
        }
        copyConstruct(items, numElements, 1, &element);
        getSegment()->sizeBytes += sizeof(T);
        return true;
    }

    [[nodiscard]] bool push_back(T&& element)
    {
        const size_t numElements = size();
        if (numElements == capacity())
        {
            if (!ensureCapacity(numElements + 1, numElements)) [[unlikely]]
            {
                return false;
            }
        }
        moveConstruct(items, numElements, 1, &element);
        getSegment()->sizeBytes += sizeof(T);
        return true;
    }

    void pop_back()
    {
        const size_t sz = size();
        SANECPP_RELEASE_ASSERT(sz > 0);
        destroyElements(items, sz - 1, 1);
        setSize(sz - 1);
    }

    void pop_front()
    {
        const size_t sz = size();
        SANECPP_RELEASE_ASSERT(sz > 0);
        moveAssignElements(items, 0, sz - 1, items + 1);
        setSize(sz - 1);
    }

    [[nodiscard]] T& front()
    {
        SANECPP_RELEASE_ASSERT(size() > 0);
        return items[0];
    }

    [[nodiscard]] const T& front() const
    {
        SANECPP_RELEASE_ASSERT(size() > 0);
        return items[0];
    }

    [[nodiscard]] T& back()
    {
        const size_t sz = size();
        SANECPP_RELEASE_ASSERT(sz > 0);
        return items[sz - 1];
    }

    [[nodiscard]] const T& back() const
    {
        const size_t sz = size();
        SANECPP_RELEASE_ASSERT(sz > 0);
        return items[sz - 1];
    }

    [[nodiscard]] bool reserve(size_t newCap) { return newCap > capacity() ? ensureCapacity(newCap, size()) : true; }
    [[nodiscard]] bool resize(size_t newSize, const T& value = T()) { return resizeInternal<true>(newSize, value); }
    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize) { return resizeInternal<false>(newSize, T()); }

    void clear()
    {
        if (items != nullptr)
        {
            destroyElements(items, 0, size());
            getSegment()->sizeBytes = 0;
        }
    }

    [[nodiscard]] bool shrink_to_fit()
    {
        const size_t sz = size();
        if (sz > 0)
        {
            if (sz != capacity())
            {
                segment* oldHeader = getSegment();
                segment* newHeader = segment::allocate(oldHeader->sizeBytes);
                if (newHeader == nullptr) [[unlikely]]
                {
                    return false;
                }
                newHeader->sizeBytes = oldHeader->sizeBytes;
                moveConstruct(newHeader->items, 0, sz, oldHeader->items);
                segment::release(oldHeader);
                items = newHeader->items;
            }
        }
        else
        {
            destruct();
        }
        return true;
    }

    [[nodiscard]] bool isEmpty() const { return (items == nullptr) || (getSegment()->memoryUsed == 0); }

    [[nodiscard]] size_t size() const
    {
        if (items == nullptr) [[unlikely]]
        {
            return 0;
        }
        else
        {
            return (getSegment()->sizeBytes / sizeof(T));
        }
    }

    [[nodiscard]] size_t capacity() const
    {
        if (items == nullptr) [[unlikely]]
        {
            return 0;
        }
        else
        {
            return (getSegment()->capacityBytes / sizeof(T));
        }
    }

    [[nodiscard]] T*       begin() { return items; }
    [[nodiscard]] const T* begin() const { return items; }
    [[nodiscard]] T*       end() { return items + size(); }
    [[nodiscard]] const T* end() const { return items + size(); }
    [[nodiscard]] T*       data() { return items; }
    [[nodiscard]] const T* data() const { return items; }

    [[nodiscard]] bool insertMove(size_t idx, T* src, size_t srcSize) { return insert<false>(idx, src, srcSize); }
    [[nodiscard]] bool insertCopy(size_t idx, const T* src, size_t srcSize) { return insert<true>(idx, src, srcSize); }
    [[nodiscard]] bool appendMove(T* src, size_t srcNumItems) { return insert<false>(size(), src, srcNumItems); }
    [[nodiscard]] bool appendCopy(const T* src, size_t srcNumItems) { return insert<true>(size(), src, srcNumItems); }
    [[nodiscard]] bool appendMove(vector<T>& src) { return appendMove(src.items, src.size()); }
    [[nodiscard]] bool appendCopy(const vector<T>& src) { return appendCopy(src.items, src.size()); }

  private:
    void destruct()
    {
        clear();
        if (items != nullptr)
        {
            segment::release(getSegment());
            items = nullptr;
        }
    }

    [[nodiscard]] bool ensureCapacity(size_t newCapacity, const size_t keepFirstN)
    {
        const auto numNewBytes = newCapacity * sizeof(T);
        const auto oldSize     = size();
        SANECPP_DEBUG_ASSERT(oldSize >= keepFirstN);
        segment* allocatedHeader = segment::allocate(numNewBytes);
        if (allocatedHeader == nullptr)
        {
            return false;
        }
        segment* newHeader   = allocatedHeader;
        newHeader->sizeBytes = static_cast<HeaderBytesType>(oldSize * sizeof(T));
        if (oldSize > 0)
        {
            segment* oldHeader = getSegment();
            moveConstruct(newHeader->items, 0, keepFirstN, items);
            destroyElements(items, keepFirstN, oldSize - keepFirstN);
            segment::release(oldHeader);
        }
        items = newHeader->items;
        return true;
    }

    static void moveAssignElements(T* destination, size_t indexStart, size_t numElements, T* source)
    {
        // TODO: Should we also call destructor on moved elements?
        for (size_t idx = indexStart; idx < (indexStart + numElements); ++idx)
            destination[idx] = move(source[idx - indexStart]);
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

    typedef decltype(segment::sizeBytes) HeaderBytesType;

    void setSize(size_t newSize) { getSegment()->sizeBytes = static_cast<HeaderBytesType>(newSize * sizeof(T)); }

    [[nodiscard]] segment* getSegment() const
    {
        return reinterpret_cast<segment*>(reinterpret_cast<uint8_t*>(items) - sizeof(segment));
    }

    bool reserveInternalTrivialAllocate(size_t newSize)
    {
        segment* newHeader;
        if (items == nullptr)
        {
            newHeader = segment::allocate(newSize * sizeof(T));
        }
        else if (newSize > capacity())
        {
            newHeader = segment::reallocate(getSegment(), newSize * sizeof(T));
        }
        else
        {
            newHeader = getSegment();
        }

        if (newHeader == nullptr)
        {
            return false;
        }
        items = newHeader->items;
        return true;
    }

    void reserveInternalTrivialInitialize(const size_t oldSize, const size_t newSize, const T& defaultValue)
    {
        if (newSize > oldSize)
        {
            int32_t val;
            memcpy(&val, &defaultValue, sizeof(int));
            if (val == 0)
            {
                memset(items + oldSize, 0, sizeof(T) * (newSize - oldSize));
            }
            else
            {
                for (size_t idx = oldSize; idx < newSize; ++idx)
                {
                    items[idx] = defaultValue;
                }
            }
        }
    }

    template <bool initialize, typename Q = T>
    [[nodiscard]] typename enable_if<is_trivially_copyable<Q>::value, bool>::type resizeInternal(size_t   newSize,
                                                                                                 const T& defaultValue)
    {
        const auto oldSize = size();

        if (!reserveInternalTrivialAllocate(newSize))
        {
            return false;
        }

        setSize(newSize);
        if (initialize)
        {
            reserveInternalTrivialInitialize(oldSize, newSize, defaultValue);
        }
        return true;
    }

    template <bool initialize, typename Q = T>
    [[nodiscard]] typename enable_if<not is_trivially_copyable<Q>::value, bool>::type resizeInternal(
        size_t newSize, const T& defaultValue)
    {
        static_assert(initialize, "There is no logical reason to skip initializing non trivially copyable class");
        if (newSize == 0)
        {
            clear();
            return false;
        }
        const auto oldSize = size();
        if (newSize > capacity())
        {
            const auto keepFirstN = min(oldSize, newSize);
            if (!ensureCapacity(newSize, keepFirstN)) [[unlikely]]
            {
                return false;
            }
            copyConstructSingle(items, keepFirstN, newSize - keepFirstN, defaultValue);
        }
        else
        {
            if (oldSize > newSize)
            {
                destroyElements(items, newSize, oldSize - newSize);
            }
            else if (oldSize < newSize)
            {
                copyConstructSingle(items, oldSize, newSize - oldSize, defaultValue);
            }
        }
        const auto numNewBytes  = newSize * sizeof(T);
        getSegment()->sizeBytes = static_cast<HeaderBytesType>(numNewBytes);
        return true;
    }

    template <typename U, bool copy>
    typename enable_if<copy, void>::type copyOrMoveConstruct(T* destination, size_t indexStart, size_t numElements,
                                                             U* sourceValue)
    {
        copyConstruct(destination, indexStart, numElements, sourceValue);
    }

    template <typename U, bool copy>
    typename enable_if<not copy, void>::type copyOrMoveConstruct(U* destination, size_t indexStart, size_t numElements,
                                                                 U* sourceValue)
    {
        moveConstruct(destination, indexStart, numElements, sourceValue);
    }

    template <bool copy, typename U>
    [[nodiscard]] bool insert(size_t position, U* other, size_t otherSize)
    {
        const size_t sz = size();
        SANECPP_RELEASE_ASSERT(position <= sz);
        if (otherSize == 0)
        {
            return true;
        }
        const size_t newSize = sz + otherSize;
        if (newSize > capacity())
        {
            if (!ensureCapacity(newSize, sz))
            {
                return false;
            }
        }
        const size_t numElementsToMove = sz - position;
        moveConstruct(items, newSize - numElementsToMove, numElementsToMove, items + position);
        copyOrMoveConstruct<U, copy>(items, position, otherSize, other);
        setSize(newSize);
        return true;
    }
};
