#pragma once
#include "Assert.h"
#include "Limits.h"
#include "Memory.h"
#include "Segment.h"
#include "Types.h"

namespace SC
{
template <typename T>
struct Vector;
struct vectorAllocator;
} // namespace SC

struct SC::vectorAllocator
{
    static SegmentHeader* reallocate(SegmentHeader* oldHeader, size_t newSize)
    {
        if (newSize > static_cast<SegmentHeader::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        SegmentHeader* newHeader =
            reinterpret_cast<SegmentHeader*>(memoryReallocate(oldHeader, sizeof(SegmentHeader) + newSize));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<SegmentHeader::HeaderBytesType>(newSize);
        }
        return newHeader;
    }

    static SegmentHeader* allocate(SegmentHeader* oldHeader, size_t numNewBytes)
    {
        if (numNewBytes > static_cast<SegmentHeader::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        SegmentHeader* newHeader =
            reinterpret_cast<SegmentHeader*>(memoryAllocate(sizeof(SegmentHeader) + numNewBytes));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<SegmentHeader::HeaderBytesType>(numNewBytes);
        }
        return newHeader;
    }

    static void release(SegmentHeader* oldHeader) { memoryRelease(oldHeader); }
};

template <typename T>
struct SC::Vector
{
    typedef SegmentOperations<vectorAllocator, T> SegmentOperations;

    T* items;

    Vector() : items(nullptr) {}
    Vector(Vector&& other) : items(other.items) { other.items = nullptr; }
    Vector(const Vector& other) : items(nullptr)
    {
        const size_t otherSize = other.size();
        if (otherSize > 0)
        {
            const bool res = appendCopy(other);
            (void)res;
            SC_DEBUG_ASSERT(res);
        }
    }
    ~Vector() { destroy(); }

    Vector& operator=(Vector&& other)
    {
        if (&other != this)
        {
            destroy();
            items       = other.items;
            other.items = nullptr;
        }
        return *this;
    }

    Vector& operator=(const Vector& other)
    {
        if (&other != this)
        {
            const bool res = SegmentOperations::copy(items, other.data(), other.size());
            (void)res;
            SC_DEBUG_ASSERT(res);
        }
        return *this;
    }

    [[nodiscard]] T& operator[](size_t index)
    {
        SC_DEBUG_ASSERT(index < size());
        return items[index];
    }

    [[nodiscard]] const T& operator[](size_t index) const
    {
        SC_DEBUG_ASSERT(index < size());
        return items[index];
    }

    [[nodiscard]] bool push_back(const T& element) { return SegmentOperations::push_back(items, element); }
    [[nodiscard]] bool push_back(T&& element) { return SegmentOperations::push_back(items, forward<T>(element)); }
    [[nodiscard]] bool pop_back()
    {
        if (items != nullptr)
            return SegmentItems<T>::getSegment(items)->pop_back();
        else
            return false;
    }
    [[nodiscard]] bool pop_front()
    {
        if (items != nullptr)
            return SegmentItems<T>::getSegment(items)->pop_front();
        else
            return false;
    }

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

    [[nodiscard]] bool reserve(size_t newCapacity)
    {
        if (newCapacity > capacity())
        {
            return SegmentOperations::ensureCapacity(items, newCapacity, size());
        }
        else
        {
            return true;
        }
    }

    [[nodiscard]] bool resize(size_t newSize, const T& value = T())
    {
        return SegmentOperations::template resizeInternal<true>(items, newSize, &value);
    }

    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        return SegmentOperations::template resizeInternal<false>(items, newSize, nullptr);
    }

    void clear()
    {
        if (items != nullptr)
        {
            SegmentItems<T>::getSegment(items)->clear();
        }
    }

    void sort()
    {
        if (items != nullptr)
        {
            SegmentItems<T>::getSegment(items)->sort();
        }
    }

    [[nodiscard]] bool shrink_to_fit() { return SegmentOperations::shrink_to_fit(items); }

    [[nodiscard]] bool isEmpty() const { return (items == nullptr) || SegmentItems<T>::getSegment(items)->isEmpty(); }

    [[nodiscard]] size_t size() const
    {
        if (items == nullptr) [[unlikely]]
        {
            return 0;
        }
        else
        {
            return SegmentItems<T>::getSegment(items)->sizeBytes / sizeof(T);
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
            return SegmentItems<T>::getSegment(items)->capacityBytes / sizeof(T);
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
        return SegmentOperations::template insert<false>(items, idx, src, srcNumItems);
    }
    [[nodiscard]] bool insertCopy(size_t idx, const T* src, size_t srcNumItems)
    {
        return SegmentOperations::template insert<true>(items, idx, src, srcNumItems);
    }
    [[nodiscard]] bool appendMove(T* src, size_t srcNumItems)
    {
        return SegmentOperations::template insert<false>(items, size(), src, srcNumItems);
    }
    [[nodiscard]] bool appendCopy(const T* src, size_t srcNumItems)
    {
        return SegmentOperations::template insert<true>(items, size(), src, srcNumItems);
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
            SegmentOperations::destroy(SegmentItems<T>::getSegment(items));
        }
        items = nullptr;
    }
};
