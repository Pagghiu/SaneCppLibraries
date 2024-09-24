// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
template <typename TagType, typename IDType = int32_t, IDType InvalidValue = -1>
struct StrongID;
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// @brief Strongly typed ID (cannot be assigned incorrectly to another ID)
/// @tparam TagType An empty class used just to tag this StrongID with a strong type
/// @tparam IDType The primitive type (typically `int` or similar) used to represent the ID
/// @tparam InvalidValue The sentinel primitive value that represents an invalid state of the ID
template <typename TagType, typename IDType, IDType InvalidValue>
struct SC::StrongID
{
    using NumericType = IDType;
    IDType identifier;

    constexpr StrongID() : identifier(InvalidValue) {}

    explicit constexpr StrongID(IDType value) : identifier(value) {}

    [[nodiscard]] constexpr bool operator==(StrongID other) const { return identifier == other.identifier; }

    [[nodiscard]] constexpr bool operator!=(StrongID other) const { return identifier != other.identifier; }

    /// @brief Check if StrongID is valid
    [[nodiscard]] constexpr bool isValid() const { return identifier != InvalidValue; }

    /// @brief Generates an unique StrongID for a given container.
    /// @tparam Container A container with a Container::contains member
    /// @param container The instance of container
    /// @return A StrongID that is not contained in Container
    template <typename Container>
    [[nodiscard]] constexpr static StrongID generateUniqueKey(const Container& container)
    {
        StrongID test = StrongID({});
        while (container.contains(test))
        {
            ++test.identifier;
        }
        return test;
    }
};

//! @}
