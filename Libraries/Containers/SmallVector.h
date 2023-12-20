// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Array.h"
#include "../Containers/Vector.h"

namespace SC
{
template <typename T, int N>
struct SmallVector;
} // namespace SC

//! @addtogroup group_containers
//! @{

/// @brief A Vector that can hold up to `N` elements inline and `> N` on heap
/// @tparam T Type of single vector element
/// @tparam N Number of elements kept inline to avoid heap allocation
///
/// SC::SmallVector is like SC::Vector but it will do heap allocation once more than `N` elements are needed. @n
/// When the `size()` becomes less than `N` the container will switch back using memory coming from inline storage.
/// @note SC::SmallVector derives from SC::Vector and it can be passed everywhere a reference to SC::Vector is needed.
/// It can be used to get rid of unnecessary allocations where the upper bound of required elements is known or it can
/// be predicted.
template <typename T, int N>
struct SC::SmallVector : public Vector<T>
{
    Array<T, N> buffer;
    SmallVector()
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF;
        static_assert(SC_COMPILER_OFFSETOF(SmallVector, buffer) == alignof(SegmentHeader), "Wrong Alignment");
        SC_COMPILER_WARNING_POP;
        init();
    }
    SmallVector(SmallVector&& other)
    {
        init();
        Vector<T>::operator=(forward<Vector<T>>(other));
    }
    SmallVector(const SmallVector& other)
    {
        init();
        Vector<T>::operator=(other);
    }
    SmallVector& operator=(SmallVector&& other)
    {
        Vector<T>::operator=(forward<Vector<T>>(other));
        return *this;
    }
    SmallVector& operator=(const SmallVector& other)
    {
        Vector<T>::operator=(other);
        return *this;
    }

    SmallVector(Vector<T>&& other)
    {
        init();
        Vector<T>::operator=(forward<Vector<T>>(other));
    }
    SmallVector(const Vector<T>& other)
    {
        init();
        Vector<T>::operator=(other);
    }
    SmallVector& operator=(Vector<T>&& other)
    {
        Vector<T>::operator=(forward<Vector<T>>(other));
        return *this;
    }
    SmallVector& operator=(const Vector<T>& other)
    {
        Vector<T>::operator=(other);
        return *this;
    }

  private:
    void init()
    {
        SegmentHeader* header = SegmentHeader::getSegmentHeader(buffer.items);
        header->isSmallVector = true;
        Vector<T>::items      = buffer.items;
    }
};
//! @}
