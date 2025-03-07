// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "../Foundation/Span.h"

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{
namespace detail
{
struct SC_COMPILER_EXPORT SegmentHeader;

struct alignas(uint64_t) SegmentHeader
{
    static constexpr uint32_t MaxCapacity = (~static_cast<uint32_t>(0)) >> 1;

    SegmentHeader(uint32_t capacity)
    {
        sizeBytes           = 0;
        unused              = 0;
        capacityBytes       = capacity;
        restoreInlineBuffer = false;
    }
    uint32_t sizeBytes : sizeof(uint32_t) * 8 - 1;
    uint32_t unused : 1;
    uint32_t capacityBytes : sizeof(uint32_t) * 8 - 1;
    uint32_t restoreInlineBuffer : 1;
};

template <bool IsArrayLayout>
struct SegmentData
{
    // clang-format off
    // volatile is needed to prevent GCC from optimizing away some memcpy calls under -O2 
    SC_COMPILER_FORCE_INLINE void* getData() volatile { return offset == 0 ? nullptr : (char*)this + offset; }
    SC_COMPILER_FORCE_INLINE const void* getData() volatile const { return offset == 0 ? nullptr : (const char*)this + offset; }
    SC_COMPILER_FORCE_INLINE void setData( void* newData) volatile { offset = newData == nullptr ? 0 : (volatile char*)newData - (volatile char*)this; }
    SC_COMPILER_FORCE_INLINE void* getInlineData() volatile { return (char*)this + sizeof(*this) + sizeof(uint64_t); }
    SC_COMPILER_FORCE_INLINE uint32_t getInlineCapacity() volatile { return static_cast<uint32_t>(*reinterpret_cast<uint64_t*>((char*)this + sizeof(*this))); }
    SC_COMPILER_FORCE_INLINE bool isInline() const { return offset == sizeof(*this) + sizeof(uint64_t); }
    // clang-format on

  protected:
    SegmentData(uint32_t capacity = 0);
    SegmentHeader header;
    ssize_t       offset = 0; // memory offset representing relative pointer to data from "this"
};

template <>
struct SegmentData<true>
{
    SC_COMPILER_FORCE_INLINE void*       getData() volatile { return (char*)this + sizeof(*this); }
    SC_COMPILER_FORCE_INLINE const void* getData() const volatile { return (const char*)this + sizeof(*this); }
    SC_COMPILER_FORCE_INLINE void        setData(void*) {}
    SC_COMPILER_FORCE_INLINE void*       getInlineData() volatile { return (char*)this + sizeof(*this); }
    SC_COMPILER_FORCE_INLINE uint32_t    getInlineCapacity() { return header.capacityBytes; }
    SC_COMPILER_FORCE_INLINE static constexpr bool isInline() { return true; }

  protected:
    SegmentData(uint32_t capacity = 0);
    SegmentHeader header;
};

/// @brief Allows SC::Segment handle trivial types
struct SC_COMPILER_EXPORT SegmentTrivial
{
    static void destruct(Span<void> data);
    static void copyConstructAs(Span<void> data, Span<const void> value);
    static void copyConstruct(Span<void> data, const void* src);
    static void copyAssign(Span<void> data, const void* src);
    static void copyInsert(Span<void> data, Span<const void> values);
    static void moveConstruct(Span<void> data, void* src);
    static void moveAssign(Span<void> data, void* src);
    static void remove(Span<void> data, size_t numElements);
};

} // namespace detail

/// @brief A slice of contiguous memory, prefixed by and header containing size and capacity.
/// Can act as a simple byte buffer or more of a "vector-like" class depending on the passed in VTable traits.
/// It transparently handles going to and from an inline buffer (defined in derived classes) and the heap.
/// @tparam VTable provides copy / move / destruct operations (see SC::SegmentTrivial as an example)
/// @note Implementation is in `.inl` to reduce include bloat for non-templated derived classes like SC::Buffer.
/// This reduces header bloat as the `.inl` can be included where derived class is defined (typically a `.cpp` file).
template <typename VTable>
struct Segment : protected detail::SegmentData<VTable::IsArray>
{
    using T = typename VTable::Type;
    Segment(uint32_t capacityInBytes);

    Segment();
    ~Segment();
    Segment(Segment&& other);
    Segment(const Segment& other);
    Segment& operator=(Segment&& other);
    Segment& operator=(const Segment& other);

    // clang-format off
    template <typename U> Segment(Span<const U> span) : Segment() { SC_ASSERT_RELEASE(assign(span)); }
    Segment(Span<const T> span) : Segment() { SC_ASSERT_RELEASE(assign(span)); }
    Segment(std::initializer_list<T> list) : Segment() { SC_ASSERT_RELEASE(assign({list.begin(), list.size()})); }
    // clang-format on

    /// @brief Re-allocates to the requested new size, preserving its contents
    /// @note To restore link with an inline buffer, resize to its capacity (or less) and call Segment::shrink
    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize);

    /// @brief Re-allocates to the requested new size, preserving its contents and setting new items to value
    [[nodiscard]] bool resize(size_t newSize, const T& value = T());

    /// @brief Reserves capacity to avoid heap-allocation during a future append, assign or resize
    [[nodiscard]] bool reserve(size_t newCapacity);

    /// @brief Appends a Span to the end of the segment
    [[nodiscard]] bool append(Span<const T> span);

    /// @brief Appends a Span of items convertible to T to the end of the segment
    template <typename U>
    [[nodiscard]] bool append(Span<const U> span);

    /// @brief Moves contents of another segment to the end of this segment
    [[nodiscard]] bool appendMove(Segment&& other);

    /// @brief Ensures `capacity == size` re-allocating (if `capacity>size`) or freeing ( if `size==0`) memory.
    /// @note If `isInlineBuffer() == true` its capacity will not shrink.
    [[nodiscard]] bool shrink_to_fit();

    /// @brief Sets size to zero without freeing any memory (use `shrink_to_fit()` to free memory)
    void clear();

    /// @brief Replaces contents with contents of the span
    /// @note This method allows detecting allocation failures (unlike the assignment operator)
    [[nodiscard]] bool assign(Span<const T> span);

    /// @brief Replaces content moving (possibly "stealing") content of another segment
    /// @note This method allows detecting allocation failures (unlike the assignment operator)
    [[nodiscard]] bool assignMove(Segment&& other);

    /// @brief Appends a single element to the end of the segment
    [[nodiscard]] bool push_back(const T& value) { return resize(size() + 1, value); }

    /// @brief Moves a single element to the end of the segment
    [[nodiscard]] bool push_back(T&& value);

    /// @brief Appends a single element to the start of the segment
    [[nodiscard]] bool push_front(const T& value) { return insert(0, value); }

    /// @brief Removes the last element of the segment
    /// @param removedValue Last item will be moved in the value if != `nullptr`
    /// @return `true` if element was successfully removed (`false` if segment is empty)
    [[nodiscard]] bool pop_back(T* removedValue = nullptr);

    /// @brief Removes the first element of the segment
    /// @param removedValue First item will be moved in the value if != `nullptr`
    /// @return `true` if element was successfully removed (`false` if segment is empty)
    [[nodiscard]] bool pop_front(T* removedValue = nullptr);

    // clang-format off
    [[nodiscard]] const T* data() const     SC_LANGUAGE_LIFETIME_BOUND { return reinterpret_cast<const T*>(Parent::getData()); }
    [[nodiscard]] T*       data()           SC_LANGUAGE_LIFETIME_BOUND { return reinterpret_cast<T*>(Parent::getData()); }

    [[nodiscard]] T*       begin()          SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] const T* begin() const    SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] T*       end()            SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }
    [[nodiscard]] const T* end() const      SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }

    [[nodiscard]] T& back()                 SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] T& front()                SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *data();}
    [[nodiscard]] const T& back() const     SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] const T& front() const    SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *data();}

    [[nodiscard]] T&       operator[](size_t idx)       SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}
    [[nodiscard]] const T& operator[](size_t idx) const SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}
    // clang-format on

    /// @brief Returns `true` if an inline buffer is in use (`false` if segment is heap allocated).
    [[nodiscard]] bool isInlineBuffer() const { return Parent::isInline(); }

    /// @brief Check if is empty (`size()` == 0)
    [[nodiscard]] bool isEmpty() const { return header.sizeBytes == 0; }

    /// @brief Obtains a Span of internal contents
    [[nodiscard]] Span<T> toSpan() SC_LANGUAGE_LIFETIME_BOUND { return {data(), size()}; }

    /// @brief Obtains a Span of internal contents
    [[nodiscard]] Span<const T> toSpanConst() const SC_LANGUAGE_LIFETIME_BOUND { return {data(), size()}; }

    /// @brief Returns current size
    [[nodiscard]] size_t size() const { return header.sizeBytes / sizeof(T); }

    /// @brief Returns current capacity (always >= of `size()`)
    [[nodiscard]] size_t capacity() { return header.capacityBytes / sizeof(T); }

    /// @brief Removes the range [start, start + length] from the segment
    /// @param start Index where the item must be removed
    /// @param length Number of elements that will be removed
    /// @return `true` if `start + length` is within bounds
    [[nodiscard]] bool removeRange(size_t start, size_t length);

    /// @brief Removes the element at index
    /// @param index Index where the item must be removed
    /// @return `true` if index is within bounds
    [[nodiscard]] bool removeAt(size_t index) { return removeRange(index, 1); }

    /// @brief Insert a span at the given index
    /// @param index Index where span should be inserted
    /// @param data Data that will be inserted  at index `idx` (by copy)
    /// @return `true` if index is less than or equal to size()
    [[nodiscard]] bool insert(size_t index, Span<const T> data);

    /// @brief Get the internal header handled by this class
    [[nodiscard]] const detail::SegmentHeader* getHeader() const { return &header; }

  protected:
    struct Internal;
    using Parent = detail::SegmentData<VTable::IsArray>;
    using Parent::header;
    using SegmentHeader = detail::SegmentHeader;
};

//! @}
} // namespace SC
