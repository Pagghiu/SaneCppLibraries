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
struct VectorAllocator;
} // namespace SC

struct SC::VectorAllocator
{
    static const size_t   SIZE_OF_VECTOR_T = 8;
    static SegmentHeader* reallocate(SegmentHeader* oldHeader, size_t newSize)
    {
        if (newSize > static_cast<SegmentHeader::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        SegmentHeader* newHeader =
            static_cast<SegmentHeader*>(memoryReallocate(oldHeader, sizeof(SegmentHeader) + newSize));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<SegmentHeader::HeaderBytesType>(newSize);
        }
        return newHeader;
    }

    static SegmentHeader* allocate(SegmentHeader* oldHeader, size_t numNewBytes, void* pself)
    {
        if (numNewBytes > static_cast<SegmentHeader::HeaderBytesType>(MaxValue()))
        {
            return nullptr;
        }
        if (oldHeader != nullptr)
        {
            if (oldHeader->options.isFollowedBySmallVector)
            {
                // If we were folloed by a small vector, we check if that small vector has enough memory
                SegmentHeader* followingHeader = static_cast<SegmentHeader*>(
                    static_cast<void*>(static_cast<char_t*>(pself) + SIZE_OF_VECTOR_T)); // vector
                if (followingHeader->options.isSmallVector && followingHeader->capacityBytes >= numNewBytes)
                {
                    return followingHeader;
                }
            }
            else if (oldHeader->options.isSmallVector)
            {
                if (oldHeader->capacityBytes >= numNewBytes)
                {
                    // shrink_to_fit on SmallVector pointing to internal buffer
                    return oldHeader;
                }
            }
        }
        SegmentHeader* newHeader = static_cast<SegmentHeader*>(memoryAllocate(sizeof(SegmentHeader) + numNewBytes));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<SegmentHeader::HeaderBytesType>(numNewBytes);
            newHeader->initDefaults();
            if (oldHeader != nullptr && oldHeader->options.isSmallVector)
            {
                newHeader->options.isFollowedBySmallVector = true;
            }
        }
        return newHeader;
    }

    static void release(SegmentHeader* oldHeader)
    {
        if (not oldHeader->options.isSmallVector)
        {
            memoryRelease(oldHeader);
        }
        else
        {
            oldHeader->sizeBytes = 0;
        }
    }
};

template <typename T>
struct SC::Vector
{
    typedef SegmentOperations<VectorAllocator, T> SegmentOperationsT;

    T* items;

    Vector() : items(nullptr) {}
    Vector(std::initializer_list<T> ilist) : items(nullptr) { (void)appendCopy(ilist.begin(), ilist.size()); }
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

    Vector(Vector&& other) noexcept
    {
        items = nullptr;
        moveAssign(forward<Vector>(other));
    }

    Vector& operator=(Vector&& other)
    {
        if (&other != this)
        {
            moveAssign(forward<Vector>(other));
        }
        return *this;
    }

    Vector& operator=(const Vector& other)
    {
        if (&other != this)
        {
            const bool res = SegmentOperationsT::copy(items, other.data(), other.size());
            (void)res;
            SC_DEBUG_ASSERT(res);
        }
        return *this;
    }

    ~Vector() { destroy(); }

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

    [[nodiscard]] bool push_back(const T& element) { return SegmentOperationsT::push_back(items, element); }
    [[nodiscard]] bool push_back(T&& element) { return SegmentOperationsT::push_back(items, forward<T>(element)); }
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
            return SegmentOperationsT::ensureCapacity(items, newCapacity, size());
        }
        else
        {
            return true;
        }
    }

    [[nodiscard]] bool resize(size_t newSize, const T& value = T())
    {
        return SegmentOperationsT::template resizeInternal<true>(items, newSize, &value);
    }

    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        return SegmentOperationsT::template resizeInternal<false>(items, newSize, nullptr);
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

    [[nodiscard]] bool shrink_to_fit() { return SegmentOperationsT::shrink_to_fit(items); }

    [[nodiscard]] bool isEmpty() const { return (items == nullptr) || SegmentItems<T>::getSegment(items)->isEmpty(); }

    [[nodiscard]] size_t size() const
    {
        if (items == nullptr)
            SC_UNLIKELY { return 0; }
        else
        {
            return SegmentItems<T>::getSegment(items)->sizeBytes / sizeof(T);
        }
    }

    [[nodiscard]] size_t capacity() const
    {
        if (items == nullptr)
            SC_UNLIKELY { return 0; }
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
        return SegmentOperationsT::template insert<false>(items, idx, src, srcNumItems);
    }
    [[nodiscard]] bool insertCopy(size_t idx, const T* src, size_t srcNumItems)
    {
        return SegmentOperationsT::template insert<true>(items, idx, src, srcNumItems);
    }
    [[nodiscard]] bool appendMove(T* src, size_t srcNumItems)
    {
        return SegmentOperationsT::template insert<false>(items, size(), src, srcNumItems);
    }
    [[nodiscard]] bool appendCopy(const T* src, size_t srcNumItems)
    {
        return SegmentOperationsT::template insert<true>(items, size(), src, srcNumItems);
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
            SegmentOperationsT::destroy(SegmentItems<T>::getSegment(items));
        }
        items = nullptr;
    }
    void moveAssign(Vector&& other)
    {
        SegmentHeader* otherHeader = other.items != nullptr ? SegmentHeader::getSegmentHeader(other.items) : nullptr;
        const bool     otherIsSmallVector = otherHeader != nullptr && otherHeader->options.isSmallVector;
        if (otherIsSmallVector)
        {
            // We can't "move" the small vector, so we just assign its items
            clear();
            (void)appendMove(other.items, other.size());
            other.clear();
        }
        else
        {
            //
            const bool otherWasFollowedBySmallVector =
                otherHeader != nullptr && otherHeader->options.isFollowedBySmallVector;
            if (otherHeader != nullptr)
            {
                // Before grabbing other.items we want to remember our state of "followed by/being a small vector"
                const SegmentHeader* oldHeader = items != nullptr ? SegmentHeader::getSegmentHeader(items) : nullptr;
                const bool           shouldStillBeFollowedBySmallVector =
                    oldHeader != nullptr &&
                    (oldHeader->options.isFollowedBySmallVector || oldHeader->options.isSmallVector);
                otherHeader->options.isFollowedBySmallVector = shouldStillBeFollowedBySmallVector;
            }

            destroy();
            items = other.items;
            if (otherWasFollowedBySmallVector)
            {
                // Other.items should become nullptr, but if it was followed by small vector, restore its link
                other.items = static_cast<T*>(static_cast<void*>(static_cast<char_t*>(static_cast<void*>(&other)) +
                                                                 sizeof(other) + sizeof(SegmentHeader)));
            }
            else
            {
                other.items = nullptr;
            }
        }
    }
};
