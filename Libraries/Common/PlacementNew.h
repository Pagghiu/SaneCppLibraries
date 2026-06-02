// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_PLACEMENT_NEW_DEFINITION_H
#if SC_FOUNDATION_PLACEMENT_NEW_DEFINITION_H != 1
#error "PlacementNew.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_PLACEMENT_NEW_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "CompilerMove.h" // forward<Q> in placementNew

// clang-format off
namespace SC
{
//! @addtogroup group_foundation_utility
//! @{
/// Tag structure for custom placement new
struct PlacementNew {};
//! @}
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// Custom placement new using SC::PlacementNew class
#if defined(_MSC_VER) && !defined(__clang__)
inline void* operator new(size_t, void* p, SC::PlacementNew) noexcept { return p; }
inline void* operator new[](size_t, void* p, SC::PlacementNew) noexcept { return p; }
inline void  operator delete(void*, void*, SC::PlacementNew) noexcept {}
#else
inline void* operator new(decltype(sizeof(0)), void* p, SC::PlacementNew) noexcept { return p; }
inline void* operator new[](decltype(sizeof(0)), void* p, SC::PlacementNew) noexcept { return p; }
#endif
namespace SC
{
/// Placement New
template<typename T, typename... Q> void placementNew(T& storage, Q&&... other) { new (&storage, PlacementNew()) T(SC::forward<Q>(other)...); }
template<typename T> void placementNewArray(T* storage, decltype(sizeof(0)) size) { new (storage, PlacementNew()) T[size]; }
template<typename T> void dtor(T& t){ t.~T(); }
}
//! @}
// clang-format on

#endif // SC_FOUNDATION_PLACEMENT_NEW_DEFINITION_H
