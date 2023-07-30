// Copyright (c) 2022-2023, Stefano Cristiano
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

    [[nodiscard]] const Container& getItems() const { return items; }

    template <typename ComparableToKey>
    [[nodiscard]] bool remove(const ComparableToKey& key)
    {
        size_t idx = 0;
        for (auto& item : items)
        {
            ++idx;
            if (item.key == key)
            {
                return items.removeAt(idx);
            }
        }
        return false;
    }
    /// Inserts an item if it doesn't exist already. If it exists or insertion fails returns false.
    [[nodiscard]] bool insertIfNotExists(Item&& item)
    {
        if (not contains(item.key))
        {
            return items.push_back(forward<Item>(item));
        }
        return false;
    }

    /// Inserts an item. If insertion fails returns nullptr.
    [[nodiscard]] Value* insertOverwrite(Item&& item)
    {
        for (auto& it : items)
        {
            if (it.key == item.key)
            {
                it.value = move(item.value);
                return &it.value;
            }
        }
        if (items.push_back(forward<Item>(item)))
        {
            return &items.back().value;
        }
        return nullptr;
    }

    [[nodiscard]] Result<Key&> insertValueUniqueKey(Value&& value)
    {
        if (items.push_back({Key::generateUniqueKey(*this), forward<Value>(value)}))
        {
            return items.back().key;
        }
        return ReturnCode{"insert error"_a8};
    }

    template <typename ComparableToKey>
    [[nodiscard]] bool contains(const ComparableToKey& key) const
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                return true;
            }
        }
        return false;
    }

    template <typename ComparableToKey>
    [[nodiscard]] bool contains(const ComparableToKey& key, const Value*& outValue) const
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                outValue = &item.value;
                return true;
            }
        }
        return false;
    }

    template <typename ComparableToKey>
    [[nodiscard]] bool contains(const ComparableToKey& key, Value*& outValue)
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                outValue = &item.value;
                return true;
            }
        }
        return false;
    }

    template <typename ComparableToKey>
    [[nodiscard]] Result<const Value&> get(const ComparableToKey& key) const
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                return item.value;
            }
        }
        return ReturnCode("Missing key"_a8);
    }

    template <typename ComparableToKey>
    [[nodiscard]] Result<Value&> get(const ComparableToKey& key)
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                return item.value;
            }
        }
        return ReturnCode("Missing key"_a8);
    }

    template <typename ComparableToKey>
    [[nodiscard]] Value* getOrCreate(const ComparableToKey& key)
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                return &item.value;
            }
        }
        if (items.push_back({key, Value()}))
        {
            return &items.back().value;
        }
        return nullptr;
    }
};
