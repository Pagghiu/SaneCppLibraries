// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Vector.h"

namespace SC
{
template <typename Value, typename Container>
struct VectorSet;
} // namespace SC

//! @addtogroup group_containers
//! @{

/// @brief A set built on an unsorted Vector, ensuring no item duplication
/// @tparam Value The contained value
/// @tparam Container The underlying container used
template <typename Value, typename Container = SC::Vector<Value>>
struct SC::VectorSet
{
    Container items;

    /// @brief Return size of the set
    auto size() const { return items.size(); }

    [[nodiscard]] Value*       begin() { return items.begin(); }
    [[nodiscard]] const Value* begin() const { return items.begin(); }
    [[nodiscard]] Value*       end() { return items.end(); }
    [[nodiscard]] const Value* end() const { return items.end(); }

    /// @brief Check if the given Value exists in the VectorSet
    template <typename ComparableToValue>
    [[nodiscard]] bool contains(const ComparableToValue& value)
    {
        return items.contains(value);
    }

    /// @brief Inserts a value in the VectorSet (if it doesn't already exists)
    [[nodiscard]] bool insert(const Value& value)
    {
        if (items.contains(value))
        {
            return true;
        }
        return items.push_back(value);
    }

    /// @brief Removes a value from the VectorSet (if it exists)
    template <typename ComparableToValue>
    [[nodiscard]] bool remove(const ComparableToValue& value)
    {
        return items.remove(value);
    }
};
//! @}
