// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Segment.h"

namespace SC
{
struct ArrayAllocator;
template <typename T, int N>
struct Array;
} // namespace SC

struct SC::ArrayAllocator
{
    [[nodiscard]] static SegmentHeader* reallocate(SegmentHeader* oldHeader, size_t newSize)
    {
        if (newSize <= oldHeader->sizeBytes)
        {
            return oldHeader;
        }
        return nullptr;
    }
    [[nodiscard]] static SegmentHeader* allocate(SegmentHeader* oldHeader, size_t numNewBytes, void* pself)
    {
        SC_COMPILER_UNUSED(numNewBytes);
        SC_COMPILER_UNUSED(pself);
        oldHeader->initDefaults();
        return oldHeader;
    }

    static void release(SegmentHeader* oldHeader) { SC_COMPILER_UNUSED(oldHeader); }

    template <typename T>
    static T* getItems(SegmentHeader* header)
    {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(header) + sizeof(SegmentHeader));
    }
    template <typename T>
    static const T* getItems(const SegmentHeader* header)
    {
        return reinterpret_cast<const T*>(reinterpret_cast<const char*>(header) + sizeof(SegmentHeader));
    }
};

template <typename T, int N>
struct SC::Array : public SegmentItems<T>
{
    using Allocator = ArrayAllocator;

  protected:
    static_assert(N > 0, "Array must have N > 0");
    using Parent     = SegmentItems<T>;
    using Operations = SegmentOperations<Allocator, T>;
    template <int>
    friend struct SmallString;
    template <typename, int>
    friend struct SmallVector;
    union
    {
        T items[N];
    };

  public:
    Array();

    Array(std::initializer_list<T> ilist);

    ~Array() { Operations::destroy(this); }

    [[nodiscard]] Span<const T> toSpanConst() const { return {items, Parent::sizeBytes / sizeof(T)}; }
    [[nodiscard]] Span<T>       toSpan() { return {items, Parent::sizeBytes / sizeof(T)}; }

    [[nodiscard]] T& front();

    [[nodiscard]] const T& front() const;

    [[nodiscard]] T& back();

    [[nodiscard]] const T& back() const;

    Array(const Array& other);

    Array(Array&& other);

    Array& operator=(const Array& other);

    Array& operator=(Array&& other);

    template <int M>
    Array(const Array<T, M>& other);

    template <int M>
    Array(Array<T, M>&& other);

    template <int M>
    Array& operator=(const Array<T, M>& other);

    template <int M>
    Array& operator=(Array<T, M>&& other);

    [[nodiscard]] bool push_back(const T& element);

    [[nodiscard]] bool push_back(T&& element);

    [[nodiscard]] bool pop_back();

    [[nodiscard]] bool pop_front();

    [[nodiscard]] T& operator[](size_t index);

    [[nodiscard]] const T& operator[](size_t index) const;

    void clear() { Operations::clear(SegmentItems<T>::getSegment(items)); }

    [[nodiscard]] bool reserve(size_t newCap) { return newCap <= Array::capacity(); }

    [[nodiscard]] bool resize(size_t newSize, const T& value = T());

    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize);

    [[nodiscard]] bool shrink_to_fit();

    [[nodiscard]] T*       begin() { return items; }
    [[nodiscard]] const T* begin() const { return items; }
    [[nodiscard]] T*       end() { return items + Array::size(); }
    [[nodiscard]] const T* end() const { return items + Array::size(); }
    [[nodiscard]] T*       data() { return items; }
    [[nodiscard]] const T* data() const { return items; }

    [[nodiscard]] bool insert(size_t idx, Span<const T> data);

    [[nodiscard]] bool append(Span<const T> data);

    template <typename U>
    [[nodiscard]] bool append(U&& src);

    template <typename U>
    [[nodiscard]] bool append(Span<U> src);

    template <typename ComparableToValue>
    [[nodiscard]] bool contains(const ComparableToValue& value, size_t* foundIndex = nullptr) const;

    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* foundIndex = nullptr) const;

    [[nodiscard]] bool removeAt(size_t index);

    template <typename Lambda>
    [[nodiscard]] bool removeAll(Lambda&& criteria);

    [[nodiscard]] bool remove(const T& value);

  private:
    [[nodiscard]] bool appendMove(Span<T> data);

    template <typename U>
    [[nodiscard]] bool appendMove(U&& src);

    [[nodiscard]] bool insertMove(size_t idx, T* src, size_t srcSize);
};

//-----------------------------------------------------------------------------------------------------------------------
// Implementation details
//-----------------------------------------------------------------------------------------------------------------------

template <typename T, int N>
SC::Array<T, N>::Array()
{
    static_assert(alignof(Array) == alignof(uint64_t), "Array Alignment");
    Array::sizeBytes     = 0;
    Array::capacityBytes = sizeof(T) * N;
}

template <typename T, int N>
SC::Array<T, N>::Array(std::initializer_list<T> ilist)
{
    Array::capacityBytes = sizeof(T) * N;
    const auto sz        = min(static_cast<int>(ilist.size()), N);
    Parent::copyConstructMultiple(items, 0, static_cast<size_t>(sz), ilist.begin());
    Parent::setSize(static_cast<size_t>(sz));
}

template <typename T, int N>
T& SC::Array<T, N>::front()
{
    const size_t numElements = Parent::size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[0];
}

template <typename T, int N>
const T& SC::Array<T, N>::front() const
{
    const size_t numElements = Parent::size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[0];
}

template <typename T, int N>
T& SC::Array<T, N>::back()
{
    const size_t numElements = Parent::size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[numElements - 1];
}

template <typename T, int N>
const T& SC::Array<T, N>::back() const
{
    const size_t numElements = Parent::size();
    SC_ASSERT_RELEASE(numElements > 0);
    return items[numElements - 1];
}
template <typename T, int N>
SC::Array<T, N>::Array(const Array& other)
{
    Array::sizeBytes     = 0;
    Array::capacityBytes = sizeof(T) * N;
    (void)append(other.items, other.size());
}

template <typename T, int N>
SC::Array<T, N>::Array(Array&& other)
{
    Array::sizeBytes     = 0;
    Array::capacityBytes = sizeof(T) * N;
    (void)appendMove(other.toSpan());
}

template <typename T, int N>
template <int M>
SC::Array<T, N>::Array(const Array<T, M>& other)
{
    static_assert(M <= N, "Unsafe operation, cannot report failure inside constructor, use append instead");
    Array::sizeBytes     = 0;
    Array::capacityBytes = sizeof(T) * N;
    (void)append(other.toSpanConst());
}

template <typename T, int N>
template <int M>
SC::Array<T, N>::Array(Array<T, M>&& other)
{
    static_assert(M <= N, "Unsafe operation, cannot report failure inside constructor, use appendMove instead");
    Array::sizeBytes     = 0;
    Array::capacityBytes = sizeof(T) * N;
    (void)appendMove(other.items, other.size());
}

template <typename T, int N>
SC::Array<T, N>& SC::Array<T, N>::operator=(const Array& other)
{
    if (&other != this)
    {
        T*   oldItems = Array::items;
        bool res      = Operations::copy(oldItems, other.items, other.size());
        (void)res;
        SC_ASSERT_DEBUG(res);
    }
    return *this;
}

template <typename T, int N>
SC::Array<T, N>& SC::Array<T, N>::operator=(Array&& other)
{
    if (&other != this)
    {
        Operations::clear(SegmentItems<T>::getSegment(items));
        if (appendMove(other.toSpan()))
        {
            Operations::clear(SegmentItems<T>::getSegment(other.items));
        }
    }
    return *this;
}

template <typename T, int N>
template <int M>
SC::Array<T, N>& SC::Array<T, N>::operator=(const Array<T, M>& other)
{
    if (&other != this)
    {
        T*   oldItems = items;
        bool res      = copy(oldItems, other.data(), other.size());
        (void)res;
        SC_ASSERT_DEBUG(res);
    }
    return *this;
}

template <typename T, int N>
template <int M>
SC::Array<T, N>& SC::Array<T, N>::operator=(Array<T, M>&& other)
{
    if (&other != this)
    {
        Array::clear();
        if (appendMove(other.items, other.size()))
        {
            other.clear();
        }
    }
    return *this;
}

template <typename T, int N>
bool SC::Array<T, N>::push_back(const T& element)
{
    T* oldItems = items;
    return Operations::push_back(oldItems, element);
}

template <typename T, int N>
bool SC::Array<T, N>::push_back(T&& element)
{
    T* oldItems = items;
    return Operations::push_back(oldItems, forward<T>(element));
}

template <typename T, int N>
bool SC::Array<T, N>::pop_back()
{
    T* oldItems = items;
    return Operations::pop_back(oldItems);
}

template <typename T, int N>
bool SC::Array<T, N>::pop_front()
{
    T* oldItems = items;
    return Operations::pop_front(oldItems);
}

template <typename T, int N>
T& SC::Array<T, N>::operator[](size_t index)
{
    SC_ASSERT_DEBUG(index < Array::size());
    return items[index];
}

template <typename T, int N>
const T& SC::Array<T, N>::operator[](size_t index) const
{
    SC_ASSERT_DEBUG(index < Array::size());
    return items[index];
}

template <typename T, int N>
bool SC::Array<T, N>::resize(size_t newSize, const T& value)
{
    T* oldItems = items;
    return Operations::template resizeInternal<true>(oldItems, newSize, &value);
}

template <typename T, int N>
bool SC::Array<T, N>::resizeWithoutInitializing(size_t newSize)
{
    T* oldItems = items;
    return Operations::template resizeInternal<false>(oldItems, newSize, nullptr);
}

template <typename T, int N>
bool SC::Array<T, N>::shrink_to_fit()
{
    T* oldItems = items;
    return Operations::shrink_to_fit(oldItems);
}

template <typename T, int N>
bool SC::Array<T, N>::insert(size_t idx, Span<const T> data)
{
    T* oldItems = items;
    return Operations::template insert<true>(oldItems, idx, data.data(), data.sizeInElements());
}

template <typename T, int N>
bool SC::Array<T, N>::append(Span<const T> data)
{
    T* oldItems = items;
    return Operations::template insert<true>(oldItems, Array::size(), data.data(), data.sizeInElements());
}

template <typename T, int N>
template <typename U>
bool SC::Array<T, N>::append(U&& src)
{
    if (appendMove({src.data(), src.size()}))
    {
        src.clear();
        return true;
    }
    return false;
}

template <typename T, int N>
template <typename U>
bool SC::Array<T, N>::append(Span<U> src)
{
    const auto oldSize = Array::size();
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
    if (oldSize + src.sizeInElements() != Array::size())
    {
        SC_TRUST_RESULT(resize(oldSize));
        return false;
    }
    return true;
}

template <typename T, int N>
template <typename ComparableToValue>
bool SC::Array<T, N>::contains(const ComparableToValue& value, size_t* foundIndex) const
{
    auto oldItems = items;
    return SegmentItems<T>::findIf(
        oldItems, 0, Array::size(), [&](const T& element) { return element == value; }, foundIndex);
}

template <typename T, int N>
template <typename Lambda>
bool SC::Array<T, N>::find(Lambda&& lambda, size_t* foundIndex) const
{
    auto oldItems = items;
    return SegmentItems<T>::findIf(oldItems, 0, Array::size(), forward<Lambda>(lambda), foundIndex);
}

template <typename T, int N>
bool SC::Array<T, N>::removeAt(size_t index)
{
    auto* oldItems = items;
    return SegmentItems<T>::removeAt(oldItems, index);
}

template <typename T, int N>
template <typename Lambda>
bool SC::Array<T, N>::removeAll(Lambda&& criteria)
{
    size_t index;
    size_t prevIndex         = 0;
    bool   atLeastOneRemoved = false;
    while (SegmentItems<T>::findIf(items, prevIndex, SegmentItems<T>::size() - prevIndex, forward<Lambda>(criteria),
                                   &index))
    {
        SC_TRY(removeAt(index));
        prevIndex         = index;
        atLeastOneRemoved = true;
    }
    return atLeastOneRemoved;
}

template <typename T, int N>
bool SC::Array<T, N>::remove(const T& value)
{
    size_t index;
    if (contains(value, &index))
    {
        return removeAt(index);
    }
    return false;
}

template <typename T, int N>
bool SC::Array<T, N>::appendMove(Span<T> data)
{
    T* oldItems = items;
    return Operations::template insert<false>(oldItems, Array::size(), data.data(), data.sizeInElements());
}

template <typename T, int N>
template <typename U>
bool SC::Array<T, N>::appendMove(U&& src)
{
    if (appendMove(src.data(), src.size()))
    {
        src.clear();
        return true;
    }
    return false;
}

template <typename T, int N>
bool SC::Array<T, N>::insertMove(size_t idx, T* src, size_t srcSize)
{
    T* oldItems = items;
    return Operations::template insert<false>(oldItems, idx, src, srcSize);
}
