// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Array.h"
#include "Vector.h"

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
        SegmentHeader* header         = SegmentHeader::getSegmentHeader(buffer.items);
        header->options.isSmallVector = true;
        Vector<T>::items              = buffer.items;
    }
    SmallVector(SmallVector&& other) : Vector<T>(forward<Vector<T>>(other)) {}
    SmallVector(const SmallVector& other) : Vector<T>(other) {}
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

    SmallVector(Vector<T>&& other) : Vector<T>(forward<Vector<T>>(other)) {}
    SmallVector(const Vector<T>& other) : Vector<T>(other) {}
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
};
