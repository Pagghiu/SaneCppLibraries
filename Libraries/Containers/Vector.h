// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "../Foundation/Limits.h"
#include "../Foundation/Memory.h"
#include "../Foundation/PrimitiveTypes.h"
#include "Internal/Segment.h"

namespace SC
{
template <typename T>
struct Vector;
struct SC_COMPILER_EXPORT VectorAllocator;
} // namespace SC
//! @defgroup group_containers Containers
//! @copybrief library_containers (see @ref library_containers for more details)

//! @addtogroup group_containers
//! @{
struct SC::VectorAllocator
{
    using SegmentHeader = Internal::SegmentHeader;
    static SegmentHeader* reallocate(SegmentHeader* oldHeader, size_t newSize);

    static SegmentHeader* allocate(SegmentHeader* oldHeader, size_t numNewBytes, void* selfPointer);

    static void release(SegmentHeader* oldHeader);

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

/// @brief A contiguous sequence of heap allocated elements
/// @tparam T Type of single vector element
///
/// All methods of SC::Vector that can fail, return a `[[nodiscard]]` `bool` (example SC::Vector::push_back). @n
/// Assignment and Copy / move construct operators will just assert as they can't return a failure code. @n
/// `memcpy` is used to optimize copies when `T` is a memcpy-able object.
/// @note Use SC::SmallVector everywhere a SC::Vector reference is needed if the upper bound size of required elements
/// is known to get rid of unnecessary heap allocations.
template <typename T>
struct SC::Vector
{
    using SegmentHeader = Internal::SegmentHeader;

  protected:
    SegmentItems<T>* getSegmentItems() const { return SegmentItems<T>::getSegment(items); }
    template <int N>
    friend struct SmallString;
    friend struct String;
    friend struct SmallStringTest;
    friend struct VectorTest;
    friend struct SmallVectorTest;
    T* items;
    using Operations = SegmentOperations<VectorAllocator, T>;

  public:
    /// @brief Constructs an empty Vector
    Vector() : items(nullptr) {}

    /// @brief Constructs a Vector from an initializer list
    /// @param list The initializer list that will be appended to this Vector
    Vector(std::initializer_list<T> list) : items(nullptr) { (void)append({list.begin(), list.size()}); }

    /// @brief Destroys the Vector, releasing allocated memory
    ~Vector() { destroy(); }

    /// @brief Copies vector into this vector
    /// @param other The vector to be copied
    Vector(const Vector& other);

    /// @brief Moves vector contents into this vector
    /// @param other The vector being moved
    Vector(Vector&& other) noexcept;

    /// @brief Move assigns another vector to this one. Contents of this vector will be freed.
    /// @param other The vector being move assigned
    /// @return Reference to this vector
    Vector& operator=(Vector&& other);

    /// @brief Copy assigns another vector to this one. Contents of this vector will be freed.
    /// @param other The vector being copy assigned
    /// @return Reference to this vector
    Vector& operator=(const Vector& other);

    /// @brief Returns a Span wrapping the entire of current vector
    /// @return a Span wrapping the entire of current vector
    [[nodiscard]] Span<const T> toSpanConst() const SC_LANGUAGE_LIFETIME_BOUND { return Span<const T>(items, size()); }

    /// @brief Returns a Span wrapping the entire of current vector
    /// @return a Span wrapping the entire of current vector
    [[nodiscard]] Span<T> toSpan() SC_LANGUAGE_LIFETIME_BOUND { return Span<T>(items, size()); }

    /// @brief Access item at index. Bounds checked in debug.
    /// @param index index of the item to be accessed
    /// @return Value at index in the vector
    [[nodiscard]] T& operator[](size_t index) SC_LANGUAGE_LIFETIME_BOUND;

    /// @brief Access item at index. Bounds checked in debug.
    /// @param index index of the item to be accessed
    /// @return Value at index in the vector
    [[nodiscard]] const T& operator[](size_t index) const SC_LANGUAGE_LIFETIME_BOUND;

    /// @brief Copies an element in front of the Vector, at position 0
    /// @param element The element to be copied in front of the Vector
    /// @return `true` if operation succeeded
    [[nodiscard]] bool push_front(const T& element) { return insert(0, {&element, 1}); }

    /// @brief Moves an element in front of the Vector, at position 0
    /// @param element The element to be moved in front of the Vector
    /// @return `true` if operation succeeded
    [[nodiscard]] bool push_front(T&& element) { return insertMove(0, {&element, 1}); }

    /// @brief Appends an element copying it at the end of the Vector
    /// @param element The element to be copied at the end of the Vector
    /// @return `true` if operation succeeded
    [[nodiscard]] bool push_back(const T& element) { return Operations::push_back(items, element); }

    /// @brief Appends an element moving it at the end of the Vector
    /// @param element The element to be moved at the end of the Vector
    /// @return `true` if operation succeeded
    [[nodiscard]] bool push_back(T&& element) { return Operations::push_back(items, move(element)); }

    /// @brief Removes the last element of the vector
    /// @return `true` if the operation succeeds
    [[nodiscard]] bool pop_back() { return Operations::pop_back(items); }

    /// @brief Removes the first element of the vector
    /// @return `true` if the operation succeeds
    [[nodiscard]] bool pop_front() { return Operations::pop_front(items); }

    /// @brief Access the first element of the Vector
    /// @return A reference to the first element of the Vector
    [[nodiscard]] T& front() SC_LANGUAGE_LIFETIME_BOUND;

    /// @brief Access the first element of the Vector
    /// @return A reference to the first element of the Vector
    [[nodiscard]] const T& front() const SC_LANGUAGE_LIFETIME_BOUND;

    /// @brief Access the last element of the Vector
    /// @return A reference to the last element of the Vector
    [[nodiscard]] T& back() SC_LANGUAGE_LIFETIME_BOUND;

    /// @brief Access the last element of the Vector
    /// @return A reference to the last element of the Vector
    [[nodiscard]] const T& back() const SC_LANGUAGE_LIFETIME_BOUND;

    /// @brief Reserves memory for newCapacity elements, allocating memory if necessary.
    /// @param newCapacity The wanted new capacity for this Vector
    /// @return `true` if memory reservation succeeded
    [[nodiscard]] bool reserve(size_t newCapacity);

    /// @brief Resizes this vector to newSize, preserving existing elements.
    /// @param newSize The wanted new size of the vector
    /// @param value a default value that will be used for new elements inserted.
    /// @return `true` if resize succeeded
    [[nodiscard]] bool resize(size_t newSize, const T& value = T());

    /// @brief Resizes this vector to newSize, preserving existing elements. Does not initialize the items between
    /// size() and capacity().
    ///         Be careful, it's up to the caller to initialize such items to avoid UB.
    /// @param newSize The wanted new size of the vector
    /// @return `true` if resize succeeded
    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize);

    /// @brief  Removes all elements from container, calling destructor for each of them.
    ///         Doesn't deallocate memory (use shrink_to_fit for that)
    void clear();

    /// @brief Sets size() to zero, without calling destructor on elements.
    void clearWithoutInitializing() { (void)resizeWithoutInitializing(0); }

    /// @brief Reallocates the vector so that size() == capacity(). If Vector is empty, it deallocates its memory.
    /// @return `true` if operation succeeded
    [[nodiscard]] bool shrink_to_fit() { return Operations::shrink_to_fit(items); }

    /// @brief Check if the vector is empty
    /// @return `true` if vector is empty.
    [[nodiscard]] bool isEmpty() const { return (items == nullptr) || getSegmentItems()->isEmpty(); }

    /// @brief Gets size of the vector
    /// @return size of the vector
    [[nodiscard]] size_t size() const;

    /// @brief Gets capacity of the vector. Capacity is always >= size.
    /// @return capacity of the vector
    [[nodiscard]] size_t capacity() const;

    /// @brief Gets pointer to first element of the vector
    /// @return pointer to first element of the vector
    [[nodiscard]] T* begin() SC_LANGUAGE_LIFETIME_BOUND { return items; }
    /// @brief Gets pointer to first element of the vector
    /// @return pointer to first element of the vector
    [[nodiscard]] const T* begin() const SC_LANGUAGE_LIFETIME_BOUND { return items; }
    /// @brief Gets pointer to one after last element of the vector
    /// @return pointer to one after last element of the vector
    [[nodiscard]] T* end() SC_LANGUAGE_LIFETIME_BOUND { return items + size(); }
    /// @brief Gets pointer to one after last element of the vector
    /// @return pointer to one after last element of the vector
    [[nodiscard]] const T* end() const SC_LANGUAGE_LIFETIME_BOUND { return items + size(); }
    /// @brief Gets pointer to first element of the vector
    /// @return pointer to first element of the vector
    [[nodiscard]] T* data() SC_LANGUAGE_LIFETIME_BOUND { return items; }
    /// @brief Gets pointer to first element of the vector
    /// @return pointer to first element of the vector
    [[nodiscard]] const T* data() const SC_LANGUAGE_LIFETIME_BOUND { return items; }

    /// @brief Inserts a range of items copying them at given index
    /// @param idx Index where to start inserting the range of items
    /// @param data the range of items to copy
    /// @return `true` if operation succeeded
    [[nodiscard]] bool insert(size_t idx, Span<const T> data);

    /// @brief Appends a range of items copying them at the end of vector
    /// @param data the range of items to copy
    /// @return `true` if operation succeeded
    [[nodiscard]] bool append(Span<const T> data);

    /// @brief Appends a range of items copying them at the end of vector
    /// @param src the range of items to copy
    /// @return `true` if operation succeeded
    template <typename U>
    [[nodiscard]] bool append(Span<const U> src);

    /// @brief Appends another vector moving its contents at the end of vector
    /// @tparam U Type of the vector to be move appended
    /// @param src The vector to be moved at end of vector
    /// @return `true` if operation succeeded
    template <typename U>
    [[nodiscard]] bool appendMove(U&& src);

    /// @brief Check if the current vector contains a given value.
    /// @tparam U Type of the object being searched
    /// @param value Value being searched
    /// @param index if passed in != `nullptr`, receives index where item was found.
    /// Only written if function returns `true`
    /// @return `true` if the vector contains the given value.
    template <typename U>
    [[nodiscard]] bool contains(const U& value, size_t* index = nullptr) const
    {
        return toSpanConst().contains(value, index);
    }

    /// @brief Finds the first item in vector matching criteria given by the lambda
    /// @tparam Lambda Type of the Lambda passed that declares a `bool operator()(const T&)` operator
    /// @param lambda The functor or lambda called that evaluates to `true` when item is found
    /// @param index if passed in != `nullptr`, receives index where item was found.
    /// @return `true` if the wanted value with given criteria is found.
    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* index = nullptr) const
    {
        return toSpanConst().find(move(lambda), index);
    }

    /// @brief Removes an item at a given index
    /// @param index Index where the item must be removed
    /// @return `true` if operation succeeded (index is within bounds)
    [[nodiscard]] bool removeAt(size_t index) { return Operations::removeAt(items, index); }

    /// @brief Removes all items matching criteria given by Lambda
    /// @tparam Lambda Type of the functor/lambda with a `bool operator()(const T&)` operator
    /// @param criteria The lambda/functor passed in
    /// @return `true` if at least one item has been removed
    template <typename Lambda>
    [[nodiscard]] bool removeAll(Lambda&& criteria);

    /// @brief Removes all values equal to `value`
    /// @tparam U Type of the Value
    /// @param value Value to be removed
    /// @return `true` if at least one item has been removed
    template <typename U>
    [[nodiscard]] bool remove(const U& value);

  private:
    [[nodiscard]] bool insertMove(size_t idx, Span<T> data);
    [[nodiscard]] bool appendMove(Span<T> data);

    void destroy();
    void moveAssign(Vector&& other);
};
//! @}

//-----------------------------------------------------------------------------------------------------------------------
// Implementation details
//-----------------------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------------------------
// VectorAllocator
//-----------------------------------------------------------------------------------------------------------------------

inline SC::Internal::SegmentHeader* SC::VectorAllocator::reallocate(SegmentHeader* oldHeader, size_t newSize)
{
    if (newSize > SegmentHeader::MaxValue)
    {
        return nullptr;
    }
    SegmentHeader* newHeader;
    if (oldHeader->isSmallVector)
    {
        newHeader          = static_cast<SegmentHeader*>(Memory::allocate(sizeof(SegmentHeader) + newSize));
        const auto minSize = min(newSize, static_cast<decltype(newSize)>(oldHeader->sizeBytes));
        ::memcpy(newHeader, oldHeader, minSize + alignof(SegmentHeader));
        newHeader->initDefaults();
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

inline SC::Internal::SegmentHeader* SC::VectorAllocator::allocate(SegmentHeader* oldHeader, size_t numNewBytes,
                                                                  void* selfPointer)
{
    if (numNewBytes > SegmentHeader::MaxValue)
    {
        return nullptr;
    }
    if (oldHeader != nullptr)
    {
        if (oldHeader->isFollowedBySmallVector)
        {
            // If we were followed by a small vector, we check if that small vector has enough memory
            SegmentHeader* followingHeader =
                reinterpret_cast<SegmentHeader*>(static_cast<char*>(selfPointer) + alignof(SegmentHeader));
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

inline void SC::VectorAllocator::release(SegmentHeader* oldHeader)
{
    if (oldHeader->isSmallVector)
    {
        oldHeader->sizeBytes = 0;
    }
    else
    {
        Memory::release(oldHeader);
    }
}

//-----------------------------------------------------------------------------------------------------------------------
// Vector
//-----------------------------------------------------------------------------------------------------------------------
template <typename T>
SC::Vector<T>::Vector(const Vector& other) : items(nullptr)
{
    static_assert(sizeof(Vector) == sizeof(void*), "sizeof(Vector)");
    const size_t otherSize = other.size();
    if (otherSize > 0)
    {
        const bool res = append(other.toSpanConst());
        (void)res;
        SC_ASSERT_DEBUG(res);
    }
}

template <typename T>
SC::Vector<T>::Vector(Vector&& other) noexcept
{
    items = nullptr;
    moveAssign(forward<Vector>(other));
}

template <typename T>
SC::Vector<T>& SC::Vector<T>::operator=(Vector&& other)
{
    if (&other != this)
    {
        moveAssign(forward<Vector>(other));
    }
    return *this;
}

template <typename T>
SC::Vector<T>& SC::Vector<T>::operator=(const Vector& other)
{
    if (&other != this)
    {
        const bool res = Operations::assign(items, other.data(), other.size());
        (void)res;
        SC_ASSERT_DEBUG(res);
    }
    return *this;
}

template <typename T>
T& SC::Vector<T>::operator[](size_t index) SC_LANGUAGE_LIFETIME_BOUND
{
    SC_ASSERT_DEBUG(index < size());
    return items[index];
}

template <typename T>
const T& SC::Vector<T>::operator[](size_t index) const SC_LANGUAGE_LIFETIME_BOUND
{
    SC_ASSERT_DEBUG(index < size());
    return items[index];
}

template <typename T>
T& SC::Vector<T>::front() SC_LANGUAGE_LIFETIME_BOUND
{
    const size_t numElements = size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[0];
}

template <typename T>
const T& SC::Vector<T>::front() const SC_LANGUAGE_LIFETIME_BOUND
{
    const size_t numElements = size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[0];
}

template <typename T>
T& SC::Vector<T>::back() SC_LANGUAGE_LIFETIME_BOUND
{
    const size_t numElements = size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[numElements - 1];
}

template <typename T>
const T& SC::Vector<T>::back() const SC_LANGUAGE_LIFETIME_BOUND
{
    const size_t numElements = size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[numElements - 1];
}

template <typename T>
bool SC::Vector<T>::reserve(size_t newCapacity)
{
    return newCapacity > capacity() ? Operations::ensureCapacity(items, newCapacity, size()) : true;
}

template <typename T>
bool SC::Vector<T>::resize(size_t newSize, const T& value)
{
    static constexpr bool IsTrivial = TypeTraits::IsTriviallyCopyable<T>::value;
    return Operations::template resizeInternal<IsTrivial, true>(items, newSize, &value);
}

template <typename T>
bool SC::Vector<T>::resizeWithoutInitializing(size_t newSize)
{
    static constexpr bool IsTrivial = TypeTraits::IsTriviallyCopyable<T>::value;
    return Operations::template resizeInternal<IsTrivial, false>(items, newSize, nullptr);
}

template <typename T>
void SC::Vector<T>::clear()
{
    if (items != nullptr)
    {
        Operations::clear(getSegmentItems());
    }
}

template <typename T>
SC::size_t SC::Vector<T>::size() const
{
    if (items == nullptr)
        SC_LANGUAGE_UNLIKELY { return 0; }
    else
    {
        return static_cast<size_t>(getSegmentItems()->sizeBytes / sizeof(T));
    }
}

template <typename T>
SC::size_t SC::Vector<T>::capacity() const
{
    if (items == nullptr)
        SC_LANGUAGE_UNLIKELY { return 0; }
    else
    {
        return static_cast<size_t>(getSegmentItems()->capacityBytes / sizeof(T));
    }
}

template <typename T>
bool SC::Vector<T>::insert(size_t idx, Span<const T> data)
{
    return Operations::template insert<true>(items, idx, data.data(), data.sizeInElements());
}

template <typename T>
bool SC::Vector<T>::append(Span<const T> data)
{
    return Operations::template insert<true>(items, size(), data.data(), data.sizeInElements());
}

template <typename T>
template <typename U>
bool SC::Vector<T>::appendMove(U&& src)
{
    if (appendMove({src.data(), src.size()}))
    {
        src.clear();
        return true;
    }
    return false;
}

// TODO: Check if this can be unified with the same version inside Segment
template <typename T>
template <typename U>
bool SC::Vector<T>::append(Span<const U> src)
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
        SC_ASSERT_RELEASE(resize(oldSize));
        return false;
    }
    return true;
}

template <typename T>
template <typename Lambda>
bool SC::Vector<T>::removeAll(Lambda&& criteria)
{
    return SegmentItems<T>::removeAll(items, 0, size(), forward<Lambda>(criteria));
}

template <typename T>
template <typename U>
bool SC::Vector<T>::remove(const U& value)
{
    return SegmentItems<T>::removeAll(items, 0, size(), [&](const auto& it) { return it == value; });
}

template <typename T>
void SC::Vector<T>::destroy()
{
    if (items != nullptr)
    {
        Operations::destroy(getSegmentItems());
    }
    items = nullptr;
}

template <typename T>
bool SC::Vector<T>::insertMove(size_t idx, Span<T> data)
{
    return Operations::template insert<false>(items, idx, data.data(), data.sizeInElements());
}

template <typename T>
bool SC::Vector<T>::appendMove(Span<T> data)
{
    return Operations::template insert<false>(items, size(), data.data(), data.sizeInElements());
}

template <typename T>
void SC::Vector<T>::moveAssign(Vector&& other)
{
    SegmentHeader* otherHeader        = other.items != nullptr ? SegmentHeader::getSegmentHeader(other.items) : nullptr;
    const bool     otherIsSmallVector = otherHeader != nullptr && otherHeader->isSmallVector;
    if (otherIsSmallVector)
    {
        // We can't "move" the small vector, so we just assign its items
        clear();
        (void)appendMove({other.items, other.size()});
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
            otherHeader = reinterpret_cast<SegmentHeader*>(reinterpret_cast<char*>(&other) + alignof(SegmentHeader));
            other.items = otherHeader->getItems<T>();
        }
        else
        {
            other.items = nullptr;
        }
    }
}
