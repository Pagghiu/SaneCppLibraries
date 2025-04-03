// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "../Foundation/Span.h"

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{
enum class SegmentAllocator
{
    Global      = 0,
    ThreadLocal = 1,
};

namespace detail
{
struct SC_COMPILER_EXPORT SegmentHeader;
struct alignas(uint64_t) SegmentHeader
{
    static constexpr uint32_t MaxCapacity = (~static_cast<uint32_t>(0)) >> 1;

    SegmentHeader(uint32_t capacity = 0, SegmentAllocator allocator = SegmentAllocator::Global)
    {
        sizeBytes     = 0;
        allocatorType = static_cast<uint32_t>(allocator);
        capacityBytes = capacity;
        hasInlineData = capacity > 0;
    }
    uint32_t sizeBytes : sizeof(uint32_t) * 8 - 1;
    uint32_t allocatorType : 1;
    uint32_t capacityBytes : sizeof(uint32_t) * 8 - 1;
    uint32_t hasInlineData : 1;
};

struct SegmentHeaderOffset
{
    using PtrOffset = size_t;
    SegmentHeader header;
    PtrOffset     offset = 0; // memory offset representing relative pointer to data from "this"
};

template <typename T>
struct SegmentSelfRelativePointer : protected SegmentHeaderOffset
{
    // clang-format off
    SC_COMPILER_FORCE_INLINE T*       data() noexcept { return offset == 0 ? nullptr : toPtr(toOffset(this) + offset); }
    SC_COMPILER_FORCE_INLINE const T* data() const noexcept { return offset == 0 ? nullptr : toPtr(toOffset(this) + offset); }
    SC_COMPILER_FORCE_INLINE bool isInline() const noexcept { return (offset == sizeof(SegmentHeaderOffset) + sizeof(uint64_t)) and header.hasInlineData; }

  protected:
    struct InlineData : public SegmentHeaderOffset // Data layout corresponds to SmallBuffer, SmallVector etc.
    {
        uint64_t capacity; // Could use uint32_t but we need to align data to 64 bit anyway
        ~InlineData() {}
        union
        {
            T data[1]; // Accessing the whole class through volatile cast anyway so array size can be whatever
        };
    };
    SC_COMPILER_FORCE_INLINE static auto toOffset(const volatile void* src) noexcept { return reinterpret_cast<PtrOffset>(src); }
    SC_COMPILER_FORCE_INLINE static T*   toPtr(PtrOffset src) noexcept { return reinterpret_cast<T*>(src); }
    SC_COMPILER_FORCE_INLINE void setData(T* mem) noexcept { offset = mem == nullptr ? 0 : toOffset(mem) - toOffset(this); }
    SC_COMPILER_FORCE_INLINE T*   getInlineData() noexcept { return (T*)reinterpret_cast<volatile InlineData*>(this)->data; }
    SC_COMPILER_FORCE_INLINE auto getInlineCapacity() noexcept { return reinterpret_cast<volatile InlineData*>(this)->capacity; }
    // clang-format on
};

/// @brief Allows SC::Segment handle trivial types
template <typename T>
struct SegmentTrivial
{
    using Type = T;
    inline static void destruct(Span<T> data) noexcept;
    // clang-format off
    template <typename U> inline static void copyConstructAs(Span<T> data, Span<const U> value) noexcept;
    template <typename U> inline static void copyConstruct(Span<T> data, const U* src) noexcept;
    template <typename U> inline static void copyAssign(Span<T> data, const U* src) noexcept;
    template <typename U> inline static void copyInsert(Span<T> data, Span<const U> values) noexcept;
    template <typename U> inline static void moveConstruct(Span<T> data, U* src) noexcept;
    template <typename U> inline static void moveAssign(Span<T> data, U* src) noexcept;
    // clang-format on
    inline static void remove(Span<T> data, size_t numElements) noexcept;
};

/// @brief Helps creating custom Segments
template <typename ParentSegment, typename CommonParent, int N = 0,
          SegmentAllocator Allocator = SegmentAllocator::ThreadLocal>
struct SegmentCustom : public ParentSegment
{
    SegmentCustom() : ParentSegment(N, Allocator) {}
    SegmentCustom(const CommonParent& other) : SegmentCustom() { CommonParent::operator=(other); }
    SegmentCustom(CommonParent&& other) : SegmentCustom() { CommonParent::operator=(move(other)); }

    SegmentCustom(const SegmentCustom& other) : SegmentCustom() { ParentSegment::operator=(other); }
    SegmentCustom(SegmentCustom&& other) : SegmentCustom() { ParentSegment::operator=(move(other)); }
    // clang-format off
    SegmentCustom& operator=(const SegmentCustom& other) { ParentSegment::operator=(other); return *this; }
    SegmentCustom& operator=(SegmentCustom&& other) { ParentSegment::operator=(move(other)); return *this; }
    // clang-format on
};
} // namespace detail

/// @brief A slice of contiguous memory, prefixed by and header containing size and capacity.
/// Can act as a simple byte buffer or more of a "vector-like" class depending on the passed in VTable traits.
/// It transparently handles going to and from an inline buffer (defined in derived classes) and the heap.
/// @tparam VTable provides copy / move / destruct operations (see SC::SegmentTrivial as an example)
/// @note Implementation is in `.inl` to reduce include bloat for non-templated derived classes like SC::Buffer.
/// This reduces header bloat as the `.inl` can be included where derived class is defined (typically a `.cpp` file).
template <typename VTable>
struct Segment : public VTable
{
    using VTable::data;
    using T = typename VTable::Type;
    Segment(uint32_t capacityInBytes, SegmentAllocator allocator = SegmentAllocator::Global) noexcept;

    Segment() noexcept;
    ~Segment() noexcept;
    Segment(Segment&& other) noexcept;
    Segment(const Segment& other) noexcept;
    Segment& operator=(Segment&& other) noexcept;
    Segment& operator=(const Segment& other) noexcept;

    // clang-format off
    template <typename U = T> Segment(Span<const U> span) noexcept : Segment() { SC_ASSERT_RELEASE(assign(span)); }
    // clang-format on
    Segment(std::initializer_list<T> list) noexcept;

    /// @brief Re-allocates to the requested new size, preserving its contents
    /// @note To restore link with an inline buffer, resize to its capacity (or less) and call Segment::shrink
    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize) noexcept;

    /// @brief Re-allocates to the requested new size, preserving its contents and setting new items to value
    [[nodiscard]] bool resize(size_t newSize, const T& value = T()) noexcept;

    /// @brief Reserves capacity to avoid heap-allocation during a future append, assign or resize
    [[nodiscard]] bool reserve(size_t capacity) noexcept;

    /// @brief Appends a Span of items convertible to T to the end of the segment
    template <typename U = T>
    [[nodiscard]] bool append(Span<const U> span) noexcept;

    /// @brief Moves contents of another segment to the end of this segment
    template <typename VTable2>
    [[nodiscard]] bool appendMove(Segment<VTable2>&& other) noexcept;

    /// @brief Ensures `capacity == size` re-allocating (if `capacity>size`) or freeing ( if `size==0`) memory.
    /// @note If `isInline() == true` its capacity will not shrink.
    [[nodiscard]] bool shrink_to_fit() noexcept;

    /// @brief Sets size to zero without freeing any memory (use `shrink_to_fit()` to free memory)
    void clear() noexcept;

    /// @brief Replaces contents with contents of the span
    /// @note This method allows detecting allocation failures (unlike the assignment operator)
    template <typename U = T>
    [[nodiscard]] bool assign(Span<const U> span) noexcept;

    /// @brief Replaces content moving (possibly "stealing") content of another segment
    /// @note This method allows detecting allocation failures (unlike the assignment operator)
    template <typename VTable2>
    [[nodiscard]] bool assignMove(Segment<VTable2>&& other) noexcept;

    /// @brief Appends a single element to the end of the segment
    [[nodiscard]] bool push_back(const T& value) noexcept { return resize(size() + 1, value); }

    /// @brief Moves a single element to the end of the segment
    [[nodiscard]] bool push_back(T&& value) noexcept;

    /// @brief Appends a single element to the start of the segment
    [[nodiscard]] bool push_front(const T& value) noexcept { return insert(0, value); }

    /// @brief Removes the last element of the segment
    /// @param removedValue Last item will be moved in the value if != `nullptr`
    /// @return `true` if element was successfully removed (`false` if segment is empty)
    [[nodiscard]] bool pop_back(T* removedValue = nullptr) noexcept;

    /// @brief Removes the first element of the segment
    /// @param removedValue First item will be moved in the value if != `nullptr`
    /// @return `true` if element was successfully removed (`false` if segment is empty)
    [[nodiscard]] bool pop_front(T* removedValue = nullptr) noexcept;

    // clang-format off
    [[nodiscard]] T*       begin()       noexcept SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] const T* begin() const noexcept SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] T*       end()         noexcept SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }
    [[nodiscard]] const T* end()   const noexcept SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }

    [[nodiscard]] T& back()              noexcept SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] T& front()             noexcept SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *data();}
    [[nodiscard]] const T& back()  const noexcept SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] const T& front() const noexcept SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_RELEASE(not isEmpty()); return *data();}

    [[nodiscard]] T&       operator[](size_t idx)       noexcept SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}
    [[nodiscard]] const T& operator[](size_t idx) const noexcept SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}
    // clang-format on

    /// @brief Check if is empty (`size()` == 0)
    [[nodiscard]] bool isEmpty() const noexcept { return VTable::header.sizeBytes == 0; }

    /// @brief Obtains a Span of internal contents
    [[nodiscard]] Span<T> toSpan() noexcept SC_LANGUAGE_LIFETIME_BOUND { return {data(), size()}; }

    /// @brief Obtains a Span of internal contents
    [[nodiscard]] Span<const T> toSpanConst() const noexcept SC_LANGUAGE_LIFETIME_BOUND { return {data(), size()}; }

    /// @brief Returns current size
    [[nodiscard]] size_t size() const noexcept { return VTable::header.sizeBytes / sizeof(T); }

    /// @brief Returns current capacity (always >= of `size()`)
    [[nodiscard]] size_t capacity() const noexcept { return VTable::header.capacityBytes / sizeof(T); }

    /// @brief Removes the range [start, start + length] from the segment
    /// @param start Index where the item must be removed
    /// @param length Number of elements that will be removed
    /// @return `true` if `start + length` is within bounds
    [[nodiscard]] bool removeRange(size_t start, size_t length) noexcept;

    /// @brief Removes the element at index
    /// @param index Index where the item must be removed
    /// @return `true` if index is within bounds
    [[nodiscard]] bool removeAt(size_t index) noexcept { return removeRange(index, 1); }

    /// @brief Insert a span at the given index
    /// @param index Index where span should be inserted
    /// @param data Data that will be inserted  at index `idx` (by copy)
    /// @return `true` if index is less than or equal to size()
    [[nodiscard]] bool insert(size_t index, Span<const T> data) noexcept;

  protected:
    template <typename VTable2>
    friend struct Segment;
    struct Internal;
};

//! @}
} // namespace SC
