// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "../Foundation/Span.h"

namespace SC
{
struct SC_COMPILER_EXPORT SegmentHeader;
struct alignas(uint64_t) SegmentHeader
{
    static constexpr uint32_t MaxCapacity = (~static_cast<uint32_t>(0)) >> 1;

    uint32_t sizeBytes : sizeof(uint32_t) * 8 - 1;
    uint32_t isInlineBuffer : 1;
    uint32_t capacityBytes : sizeof(uint32_t) * 8 - 1;
    uint32_t isFollowedByInlineBuffer : 1;
    // clang-format off
    template <typename T>       T* getData()       { return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + sizeof(*this)); }
    template <typename T> const T* getData() const { return reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + sizeof(*this)); }
    // clang-format on
};

//! @addtogroup group_foundation_utility
//! @{

/// @brief A slice of contiguous memory, prefixed by and header containing size and capacity.
/// Can act as a simple byte buffer or more of a "vector-like" class depending on the passed in VTable traits.
/// It transparently handles going to and from an inline buffer (defined in derived classes) and the heap.
/// @tparam VTable provides copy / move / destruct operations (see SC::SegmentTrivial as an example)
/// @note Implementation is in `.inl` to reduce include bloat for non-templated derived classes like SC::Buffer.
/// This reduces header bloat as the `.inl` can be included where derived class is defined (typically a `.cpp` file).
template <typename VTable>
struct Segment
{
    using T = typename VTable::Type;

    Segment();
    ~Segment();
    Segment(Segment&& other);
    Segment(const Segment& other);
    Segment& operator=(Segment&& other);
    Segment& operator=(const Segment& other);

    template <typename U>
    Segment(Span<const U> span) : Segment()
    {
        SC_ASSERT_RELEASE(assign(span));
    }

    Segment(Span<const T> span) : Segment() { SC_ASSERT_RELEASE(assign(span)); }

    Segment(std::initializer_list<T> list) : Segment() { SC_ASSERT_RELEASE(assign({list.begin(), list.size()})); }

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

    /// @brief Access data owned by the segment or `nullptr` if segment is empty
    [[nodiscard]] const T* data() const SC_LANGUAGE_LIFETIME_BOUND
    {
        return header == nullptr ? nullptr : reinterpret_cast<const T*>(reinterpret_cast<const char*>(header) + sizeof(SegmentHeader));
    }

    /// @brief Access data owned by the segment  or `nullptr` if segment is empty
    [[nodiscard]] T* data() SC_LANGUAGE_LIFETIME_BOUND
    {
        return header == nullptr ? nullptr : reinterpret_cast<T*>(reinterpret_cast<char*>(header) + sizeof(SegmentHeader));
    }

    [[nodiscard]] T*       begin()          SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] const T* begin() const    SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] T*       end()            SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }
    [[nodiscard]] const T* end() const      SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }

    [[nodiscard]] T& back()                 SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] T& front()                SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *data();}
    [[nodiscard]] T& operator[](size_t idx) SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}
    [[nodiscard]] const T& back() const     SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] const T& front() const    SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *data();}
    [[nodiscard]] const T& operator[](size_t idx) const SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}

    // clang-format on

    /// @brief Returns `true` if an inline buffer is in use (`false` if segment is heap allocated).
    [[nodiscard]] bool isInlineBuffer() const { return header != nullptr and header->isInlineBuffer; }

    /// @brief Check if is empty (`size()` == 0)
    [[nodiscard]] bool isEmpty() const { return header == nullptr or header->sizeBytes == 0; }

    /// @brief Obtains a Span of internal contents
    [[nodiscard]] Span<T> toSpan() SC_LANGUAGE_LIFETIME_BOUND { return {data(), size()}; }

    /// @brief Obtains a Span of internal contents
    [[nodiscard]] Span<const T> toSpanConst() const SC_LANGUAGE_LIFETIME_BOUND { return {data(), size()}; }

    /// @brief Returns current size
    [[nodiscard]] size_t size() const { return header == nullptr ? 0 : header->sizeBytes / sizeof(T); }

    /// @brief Returns current capacity (always >= of `size()`)
    [[nodiscard]] size_t capacity() { return header == nullptr ? 0 : header->capacityBytes / sizeof(T); }

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

    /// @brief Sets the internal header handled by this class
    void unsafeSetHeader(SegmentHeader* newHeader) { header = newHeader; }

    /// @brief Get the internal header handled by this class
    [[nodiscard]] SegmentHeader* unsafeGetHeader() const { return header; }

    /// @brief Builds a Segment with an inlineHeader of given capacity in bytes
    Segment(SegmentHeader& inlineHeader, uint32_t capacityInBytes);

  protected:
    struct Internal;
    // Alignment is relevant for 32 bit platforms
    alignas(alignof(SegmentHeader)) SegmentHeader* header;
};

/// @brief Allows SC::Segment handle trivial types
struct SC_COMPILER_EXPORT SegmentTrivial
{
    static void destruct(SegmentHeader& header, size_t bytesOffset, size_t numBytes);
    static void copyConstructSingle(SegmentHeader& dest, size_t bytesOffset, const void* value, size_t numBytes,
                                    size_t valueSize);
    static void copyConstruct(SegmentHeader& dest, size_t bytesOffset, const void* src, size_t numBytes);
    static void copyAssign(SegmentHeader& dest, size_t bytesOffset, const void* src, size_t numBytes);
    static void copyInsert(SegmentHeader& dest, size_t bytesOffset, const void* src, size_t numBytes);
    static void moveConstruct(SegmentHeader& dest, size_t bytesOffset, void* src, size_t numBytes);
    static void moveAssign(SegmentHeader& dest, size_t bytesOffset, void* src, size_t numBytes);
    static void remove(SegmentHeader& dest, size_t fromBytesOffset, size_t toBytesOffset);
};

/// @brief Basic allocator for SC::Segment using Memory functions
struct SC_COMPILER_EXPORT SegmentAllocator
{
    static SegmentHeader* allocateNewHeader(size_t newCapacityInBytes);
    static SegmentHeader* reallocateExistingHeader(SegmentHeader& src, size_t newCapacityInBytes);

    static void destroyHeader(SegmentHeader& header);
};
//! @}
} // namespace SC
