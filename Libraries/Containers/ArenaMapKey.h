// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"

namespace SC
{
template <typename T>
struct ArenaMap;
template <typename T>
struct ArenaMapKey;
} // namespace SC

//! @addtogroup group_containers
//! @{

/// @brief A sparse vector keeping objects at a stable memory location.
///         All operations return an SC::ArenaMapKey that can be used to recover values in constant time.
/// @tparam T Type of items kept in this Arena
template <typename T>
struct SC::ArenaMapKey
{
  private:
    struct SC_COMPILER_EXPORT Generation
    {
        uint32_t used       : 1;
        uint32_t generation : 31;
        Generation()
        {
            used       = 0;
            generation = 0;
        }
        bool operator!=(const Generation other) const { return used != other.used or generation != other.generation; }
    };
    Generation generation;
    uint32_t   index;
    friend struct ArenaMap<T>;
    template <typename U>
    friend struct ArenaMapKey;

  public:
    ArenaMapKey() { index = 0; }

    bool isValid() const { return generation.used != 0; }

    template <typename U>
    ArenaMapKey<U> cast_to()
    {
        ArenaMapKey<U> key;
        key.generation.used       = generation.used;
        key.generation.generation = generation.generation;
        key.index                 = index;
        return key;
    }

    template <typename U>
    bool operator==(ArenaMapKey<U> other) const
    {
        return index == other.index and generation.used == other.generation.used and
               generation.generation == other.generation.generation;
    }

    static constexpr uint32_t MaxGenerations = (uint32_t(1) << 31) - 1;
    static constexpr uint32_t MaxIndex       = 0xffffffff;
};
//! @}
