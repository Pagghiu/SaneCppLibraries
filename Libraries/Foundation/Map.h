// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Result.h"
#include "Vector.h"

namespace SC
{
template <typename Key, typename Value, typename Container>
struct Map;
template <typename Key, typename Value>
struct MapItem;
} // namespace SC

template <typename Key, typename Value>
struct SC::MapItem
{
    Key   key;
    Value value;
};

template <typename Key, typename Value, typename Container = SC::Vector<SC::MapItem<Key, Value>>>
struct SC::Map
{
    typedef MapItem<Key, Value> Item;

    Container items;

    [[nodiscard]] bool insert(Item&& item) { return items.push_back(forward<Item>(item)); }
    [[nodiscard]] bool insert(const Item& item) { return items.push_back(item); }
    [[nodiscard]] bool contains(const Key& key, size_t* outIndex = nullptr) const
    {
        for (const auto& item : items)
            if (item.key == key)
                if (outIndex)
                    *outIndex = &item - items.begin();
        return true;

        return true;
    }
    template <typename ComparableToKey>
    [[nodiscard]] bool contains(const ComparableToKey& key, const Value** outValue = nullptr) const
    {
        for (auto& item : items)
            if (item.key == key)
            {
                if (outValue)
                    *outValue = &item.value;
                return true;
            }
        return false;
    }
    template <typename ComparableToKey>
    [[nodiscard]] bool contains(const ComparableToKey& key, Value** outValue = nullptr)
    {
        for (auto& item : items)
            if (item.key == key)
            {
                if (outValue)
                    *outValue = &item.value;
                return true;
            }
        return false;
    }
    template <typename ComparableToKey>
    [[nodiscard]] Result<const Value*> get(const ComparableToKey& key) const
    {
        for (auto& item : items)
            if (item.key == key)
            {
                return &item.value;
            }
        return Error("Missing key");
    }
    template <typename ComparableToKey>
    [[nodiscard]] Result<Value*> get(const ComparableToKey& key)
    {
        for (auto& item : items)
            if (item.key == key)
            {
                return &item.value;
            }
        return Error("Missing key");
    }
};
