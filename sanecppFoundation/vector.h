#pragma once
#include "assert.h"
#include "limits.h"
#include "memory.h"
#include "segment.h"
#include "types.h"

namespace sanecpp
{
template <typename T>
struct vector;
struct vectorAllocator;
} // namespace sanecpp

struct sanecpp::vectorAllocator
{
    static segmentHeader* reallocate(segmentHeader* oldHeader, size_t newSize)
    {
        if (newSize > static_cast<segmentHeader::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        segmentHeader* newHeader =
            reinterpret_cast<segmentHeader*>(memoryReallocate(oldHeader, sizeof(segmentHeader) + newSize));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<segmentHeader::HeaderBytesType>(newSize);
        }
        return newHeader;
    }

    static segmentHeader* allocate(segmentHeader* oldHeader, size_t numNewBytes)
    {
        if (numNewBytes > static_cast<segmentHeader::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        segmentHeader* newHeader =
            reinterpret_cast<segmentHeader*>(memoryAllocate(sizeof(segmentHeader) + numNewBytes));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<segmentHeader::HeaderBytesType>(numNewBytes);
        }
        return newHeader;
    }

    static void release(segmentHeader* oldHeader) { memoryRelease(oldHeader); }
};

template <typename T>
struct sanecpp::vector
{
    typedef segmentOperations<vectorAllocator, T> segmentOperations;

    T* items;

    vector() : items(nullptr) {}
    vector(vector&& other) : items(other.items) { other.items = nullptr; }
    vector(const vector& other) : items(nullptr)
    {
        const size_t otherSize = other.size();
        if (otherSize > 0)
        {
            const bool res = appendCopy(other);
            (void)res;
            SANECPP_DEBUG_ASSERT(res);
        }
    }
    ~vector() { destroy(); }

    vector& operator=(vector&& other)
    {
        if (&other != this)
        {
            destroy();
            items       = other.items;
            other.items = nullptr;
        }
        return *this;
    }

    vector& operator=(const vector& other)
    {
        if (&other != this)
        {
            const bool res = segmentOperations::copy(items, other.data(), other.size());
            (void)res;
            SANECPP_DEBUG_ASSERT(res);
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

    [[nodiscard]] bool push_back(const T& element) { return segmentOperations::push_back(items, element); }
    [[nodiscard]] bool push_back(T&& element) { return segmentOperations::push_back(items, forward<T>(element)); }
    [[nodiscard]] bool pop_back()
    {
        if (items != nullptr)
            return segmentItems<T>::getSegment(items)->pop_back();
        else
            return false;
    }
    [[nodiscard]] bool pop_front()
    {
        if (items != nullptr)
            return segmentItems<T>::getSegment(items)->pop_front();
        else
            return false;
    }

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

    [[nodiscard]] bool reserve(size_t newCapacity)
    {
        if (newCapacity > capacity())
        {
            return segmentOperations::ensureCapacity(items, newCapacity, size());
        }
        else
        {
            return true;
        }
    }

    [[nodiscard]] bool resize(size_t newSize, const T& value = T())
    {
        return segmentOperations::template resizeInternal<true>(items, newSize, &value);
    }

    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        return segmentOperations::template resizeInternal<false>(items, newSize, nullptr);
    }

    void clear()
    {
        if (items != nullptr)
        {
            segmentItems<T>::getSegment(items)->clear();
        }
    }

    [[nodiscard]] bool shrink_to_fit() { return segmentOperations::shrink_to_fit(items); }

    [[nodiscard]] bool isEmpty() const { return (items == nullptr) || segmentItems<T>::getSegment(items)->isEmpty(); }

    [[nodiscard]] size_t size() const
    {
        if (items == nullptr) [[unlikely]]
        {
            return 0;
        }
        else
        {
            return segmentItems<T>::getSegment(items)->sizeBytes / sizeof(T);
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
            return segmentItems<T>::getSegment(items)->capacityBytes / sizeof(T);
        }
    }

    [[nodiscard]] T*       begin() { return items; }
    [[nodiscard]] const T* begin() const { return items; }
    [[nodiscard]] T*       end() { return items + size(); }
    [[nodiscard]] const T* end() const { return items + size(); }
    [[nodiscard]] T*       data() { return items; }
    [[nodiscard]] const T* data() const { return items; }

    [[nodiscard]] bool insertMove(size_t idx, T* src, size_t srcNumItems)
    {
        return segmentOperations::template insert<false>(items, idx, src, srcNumItems);
    }
    [[nodiscard]] bool insertCopy(size_t idx, const T* src, size_t srcNumItems)
    {
        return segmentOperations::template insert<true>(items, idx, src, srcNumItems);
    }
    [[nodiscard]] bool appendMove(T* src, size_t srcNumItems)
    {
        return segmentOperations::template insert<false>(items, size(), src, srcNumItems);
    }
    [[nodiscard]] bool appendCopy(const T* src, size_t srcNumItems)
    {
        return segmentOperations::template insert<true>(items, size(), src, srcNumItems);
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

  private:
    void destroy()
    {
        if (items != nullptr)
        {
            segmentOperations::destroy(segmentItems<T>::getSegment(items));
        }
        items = nullptr;
    }
};
