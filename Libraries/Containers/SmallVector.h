// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
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
///
/// \snippet Libraries/Containers/Tests/SmallVectorTest.cpp SmallVectorSnippet
template <typename T, int N>
struct SC::SmallVector : public Vector<T>
{
    // clang-format off
    SmallVector() : Vector<T>(inlineHeader, N * sizeof(T)) {}
    SmallVector(const Vector<T>& other) : SmallVector() { Vector<T>::operator=(other); }
    SmallVector(Vector<T>&& other) : SmallVector() { Vector<T>::operator=(move(other)); }
    Vector<T>& operator=(const Vector<T>& other) { return Vector<T>::operator=(other); }
    Vector<T>& operator=(Vector<T>&& other) { return Vector<T>::operator=(move(other)); }

    SmallVector(const SmallVector& other) : SmallVector() { Vector<T>::operator=(other); }
    SmallVector(SmallVector&& other) : SmallVector() { Vector<T>::operator=(move(other)); }
    SmallVector& operator=(const SmallVector& other) { Vector<T>::operator=(other); return *this; }
    SmallVector& operator=(SmallVector&& other) { Vector<T>::operator=(move(other)); return *this; }
    // clang-format on

  private:
    SegmentHeader inlineHeader;
    char          inlineBuffer[N * sizeof(T)];
};
//! @}
