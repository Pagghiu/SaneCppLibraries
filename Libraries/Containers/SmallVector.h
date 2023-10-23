// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Array.h"
#include "../Containers/Vector.h"

namespace SC
{
template <typename T, int N>
struct SmallVector;
} // namespace SC

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
