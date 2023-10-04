// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Assert.h"
#include "../Base/Limits.h"
#include "../Base/Memory.h"
#include "../Base/Types.h"
#include "../Language/Result.h"
#include "Segment.h"

namespace SC
{
template <typename T>
struct Vector;
struct VectorAllocator;
} // namespace SC

struct SC_COMPILER_EXPORT SC::VectorAllocator
{
    static const size_t   SIZE_OF_VECTOR_T = 8;
    static SegmentHeader* reallocate(SegmentHeader* oldHeader, size_t newSize)
    {
        if (newSize > SegmentHeader::MaxValue)
        {
            return nullptr;
        }
        SegmentHeader* newHeader;
        if (oldHeader->isSmallVector)
        {
            newHeader = static_cast<SegmentHeader*>(Memory::allocate(sizeof(SegmentHeader) + newSize));
            memcpy(newHeader, oldHeader,
                   sizeof(SegmentHeader) + min(newSize, static_cast<decltype(newSize)>(oldHeader->sizeBytes)));
            newHeader->initDefaults();
            newHeader->capacityBytes           = static_cast<decltype(SegmentHeader::capacityBytes)>(newSize);
            newHeader->isFollowedBySmallVector = true;
        }
        else
        {
            newHeader = static_cast<SegmentHeader*>(Memory::reallocate(oldHeader, sizeof(SegmentHeader) + newSize));
        }
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<SegmentHeader::SizeType>(newSize);
        }
        return newHeader;
    }

    static SegmentHeader* allocate(SegmentHeader* oldHeader, size_t numNewBytes, void* pself)
    {
        if (numNewBytes > SegmentHeader::MaxValue)
        {
            return nullptr;
        }
        if (oldHeader != nullptr)
        {
            if (oldHeader->isFollowedBySmallVector)
            {
                // If we were folloed by a small vector, we check if that small vector has enough memory
                SegmentHeader* followingHeader =
                    reinterpret_cast<SegmentHeader*>(static_cast<char*>(pself) + SIZE_OF_VECTOR_T); // vector
                if (followingHeader->isSmallVector && followingHeader->capacityBytes >= numNewBytes)
                {
                    return followingHeader;
                }
            }
            else if (oldHeader->isSmallVector)
            {
                if (oldHeader->capacityBytes >= numNewBytes)
                {
                    // shrink_to_fit on SmallVector pointing to internal buffer
                    return oldHeader;
                }
            }
        }
        SegmentHeader* newHeader = static_cast<SegmentHeader*>(Memory::allocate(sizeof(SegmentHeader) + numNewBytes));
        if (newHeader)
        {
            newHeader->capacityBytes = static_cast<SegmentHeader::SizeType>(numNewBytes);
            newHeader->initDefaults();
            if (oldHeader != nullptr && oldHeader->isSmallVector)
            {
                newHeader->isFollowedBySmallVector = true;
            }
        }
        return newHeader;
    }

    static void release(SegmentHeader* oldHeader)
    {
        if (not oldHeader->isSmallVector)
        {
            Memory::release(oldHeader);
        }
        else
        {
            oldHeader->sizeBytes = 0;
        }
    }

    template <typename T>
    static T* getItems(SegmentHeader* header)
    {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(header) + sizeof(SegmentHeader));
    }
    template <typename T>
    static const T* getItems(const SegmentHeader* header)
    {
        return reinterpret_cast<T*>(reinterpret_cast<const char*>(header) + sizeof(SegmentHeader));
    }
};

template <typename T>
struct SC::Vector
{
    using SegmentOperationsT = SegmentOperations<VectorAllocator, T>;

    T* items;

    Vector() : items(nullptr) {}
    Vector(std::initializer_list<T> ilist) : items(nullptr) { (void)appendCopy(ilist.begin(), ilist.size()); }
    Vector(const Vector& other) : items(nullptr)
    {
        static_assert(sizeof(Vector) == sizeof(void*), "asd");
        const size_t otherSize = other.size();
        if (otherSize > 0)
        {
            const bool res = appendCopy(other);
            (void)res;
            SC_ASSERT_DEBUG(res);
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
            SC_ASSERT_DEBUG(res);
        }
        return *this;
    }

    ~Vector() { destroy(); }

    Span<const T> toSpanConst() const { return {items, size() * sizeof(T)}; }
    Span<T>       toSpan() { return {items, size() * sizeof(T)}; }

    // Reinterpret Vector to hold a different type (example Vector<int32_t> to Vector<int8_t>.
    // This works because the SegmentHeader stores item size and capacity in bytes
    template <typename Q>
    Vector<const Q>& unsafeReinterpretAsConst() const
    {
        return *reinterpret_cast<Vector<const Q>*>(this);
    }
    // Reinterpret Vector to hold a different type (example Vector<int32_t> to Vector<int8_t>.
    // This works because the SegmentHeader stores item size and capacity in bytes
    template <typename Q>
    Vector<Q>& unsafeReinterpretAs()
    {
        return *reinterpret_cast<Vector<Q>*>(this);
    }

    [[nodiscard]] T& operator[](size_t index)
    {
        SC_ASSERT_DEBUG(index < size());
        return items[index];
    }

    [[nodiscard]] const T& operator[](size_t index) const
    {
        SC_ASSERT_DEBUG(index < size());
        return items[index];
    }

    [[nodiscard]] bool push_front(const T& element) { return insertCopy(0, &element, 1); }
    [[nodiscard]] bool push_back(const T& element) { return SegmentOperationsT::push_back(items, element); }
    [[nodiscard]] bool push_back(T&& element) { return SegmentOperationsT::push_back(items, forward<T>(element)); }

    // TODO: Check if this can be unified with the same version inside Segment
    template <typename U>
    [[nodiscard]] bool push_back(Span<U> src)
    {
        const auto oldSize = size();
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
        if (oldSize + src.sizeInElements() != size())
        {
            SC_TRUST_RESULT(resize(oldSize));
            return false;
        }
        return true;
    }

    [[nodiscard]] bool pop_back() { return SegmentOperationsT::pop_back(items); }
    [[nodiscard]] bool pop_front() { return SegmentOperationsT::pop_front(items); }

    [[nodiscard]] T& front()
    {
        const size_t numElements = size();
        SC_ASSERT_RELEASE(numElements > 0);
        return items[0];
    }

    [[nodiscard]] const T& front() const
    {
        const size_t numElements = size();
        SC_ASSERT_RELEASE(numElements > 0);
        return items[0];
    }

    [[nodiscard]] T& back()
    {
        const size_t numElements = size();
        SC_ASSERT_RELEASE(numElements > 0);
        return items[numElements - 1];
    }

    [[nodiscard]] const T& back() const
    {
        const size_t numElements = size();
        SC_ASSERT_RELEASE(numElements > 0);
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
            SegmentOperationsT::clear(SegmentItems<T>::getSegment(items));
        }
    }

    void clearWithoutInitializing() { (void)resizeWithoutInitializing(0); }

    [[nodiscard]] bool shrink_to_fit() { return SegmentOperationsT::shrink_to_fit(items); }

    [[nodiscard]] bool isEmpty() const { return (items == nullptr) || SegmentItems<T>::getSegment(items)->isEmpty(); }

    [[nodiscard]] size_t size() const
    {
        if (items == nullptr)
            SC_LANGUAGE_UNLIKELY { return 0; }
        else
        {
            return static_cast<size_t>(SegmentItems<T>::getSegment(items)->sizeBytes / sizeof(T));
        }
    }

    [[nodiscard]] size_t capacity() const
    {
        if (items == nullptr)
            SC_LANGUAGE_UNLIKELY { return 0; }
        else
        {
            return static_cast<size_t>(SegmentItems<T>::getSegment(items)->capacityBytes / sizeof(T));
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

    [[nodiscard]] bool appendCopy(std::initializer_list<T> src) { return appendCopy(src.begin(), src.size()); }

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

    template <typename U>
    [[nodiscard]] bool contains(const U& value, size_t* foundIndex = nullptr) const
    {
        return SegmentItems<T>::findIf(
            items, 0, size(), [&](const T& element) { return element == value; }, foundIndex);
    }

    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* foundIndex = nullptr) const
    {
        return SegmentItems<T>::findIf(items, 0, size(), forward<Lambda>(lambda), foundIndex);
    }

    [[nodiscard]] bool removeAt(size_t index) { return SegmentOperationsT::removeAt(items, index); }

    template <typename Lambda>
    [[nodiscard]] bool removeAll(Lambda&& criteria)
    {
        size_t index;
        size_t prevIndex         = 0;
        bool   atLeastOneRemoved = false;
        while (SegmentItems<T>::findIf(items, prevIndex, size() - prevIndex, forward<Lambda>(criteria), &index))
        {
            SC_TRY(removeAt(index));
            prevIndex         = index;
            atLeastOneRemoved = true;
        }
        return atLeastOneRemoved;
    }

    template <typename ComparableToValue>
    [[nodiscard]] bool remove(const ComparableToValue& value)
    {
        size_t index;
        if (contains(value, &index))
        {
            return removeAt(index);
        }
        return false;
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
        const bool     otherIsSmallVector = otherHeader != nullptr && otherHeader->isSmallVector;
        if (otherIsSmallVector)
        {
            // We can't "move" the small vector, so we just assign its items
            clear();
            (void)appendMove(other.items, other.size());
            other.clear();
        }
        else
        {
            const bool otherWasFollowedBySmallVector = otherHeader != nullptr && otherHeader->isFollowedBySmallVector;
            if (otherHeader != nullptr)
            {
                // Before grabbing other.items we want to remember our state of "followed by/being a small vector"
                const SegmentHeader* oldHeader = items != nullptr ? SegmentHeader::getSegmentHeader(items) : nullptr;
                const bool           shouldStillBeFollowedBySmallVector =
                    oldHeader != nullptr && (oldHeader->isFollowedBySmallVector || oldHeader->isSmallVector);
                otherHeader->isFollowedBySmallVector = shouldStillBeFollowedBySmallVector;
            }

            destroy();
            items = other.items;
            if (otherWasFollowedBySmallVector)
            {
                // Other.items should become nullptr, but if it was followed by small vector, restore its link
                // The Array<> holding the small buffer MUST be placed after the vector.
                // We should advance by sizeof(Vector<T>) that is sizeof(void*) but on 32 bit we will still have
                // some padding, as Array<> inherits from SegmentHeader, so it shares the same alignment (that is 8
                // bytes on all platforms).
                otherHeader =
                    reinterpret_cast<SegmentHeader*>(reinterpret_cast<char*>(&other) + alignof(SegmentHeader));
                other.items = otherHeader->getItems<T>();
            }
            else
            {
                other.items = nullptr;
            }
        }
    }
};
