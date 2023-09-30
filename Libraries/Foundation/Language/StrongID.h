// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Types.h"
namespace SC
{
template <typename TagType, typename IDType = int32_t, IDType InvalidValue = -1>
struct StrongID
{
    IDType identifier;

    constexpr StrongID() : identifier(InvalidValue) {}

    explicit constexpr StrongID(IDType value) : identifier(value) {}

    [[nodiscard]] constexpr SC_COMPILER_FORCE_INLINE bool operator==(StrongID other) const
    {
        return identifier == other.identifier;
    }

    [[nodiscard]] constexpr SC_COMPILER_FORCE_INLINE bool operator!=(StrongID other) const
    {
        return identifier != other.identifier;
    }

    [[nodiscard]] constexpr SC_COMPILER_FORCE_INLINE bool isValid() const { return identifier != InvalidValue; }

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
} // namespace SC
