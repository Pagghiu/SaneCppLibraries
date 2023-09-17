// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Vector.h"
#include "../Objects/Result.h"

namespace SC
{
template <typename Value, typename Container>
struct Set;
} // namespace SC

template <typename Value, typename Container = SC::Vector<Value>>
struct SC::Set
{
    Container items;

    auto size() const { return items.size(); }

    [[nodiscard]] Value*       begin() { return items.begin(); }
    [[nodiscard]] const Value* begin() const { return items.begin(); }
    [[nodiscard]] Value*       end() { return items.end(); }
    [[nodiscard]] const Value* end() const { return items.end(); }

    template <typename ComparableToValue>
    [[nodiscard]] bool contains(const ComparableToValue& value)
    {
        return items.contains(value);
    }

    [[nodiscard]] bool insert(const Value& value)
    {
        if (items.contains(value))
        {
            return true;
        }
        return items.push_back(value);
    }

    template <typename ComparableToValue>
    [[nodiscard]] bool remove(const ComparableToValue& value)
    {
        return items.remove(value);
    }
};
