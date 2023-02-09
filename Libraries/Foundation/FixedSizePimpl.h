// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Language.h"

namespace SC
{
template <typename T, int N>
struct FixedSizePimpl
{
    FixedSizePimpl()
    {
        static_assert(N >= sizeof(T), "Increase size of static pimpl");
        new (buffer, PlacementNew()) T;
        destruct = [](T& t) { t.~T(); };
    }
    ~FixedSizePimpl() { destruct(get()); }

    T&       get() { return reinterpret_cast<T&>(buffer); }
    const T& get() const { return reinterpret_cast<const T&>(buffer); }

  private:
    typedef void (*FixedSizePimplDestruct)(T& obj);
    FixedSizePimplDestruct destruct = nullptr;

    alignas(uint64_t) uint8_t buffer[N];
};
} // namespace SC
