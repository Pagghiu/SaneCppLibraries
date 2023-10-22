// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Vector.h"

namespace SC
{
template <typename Key, typename Value, typename Container>
struct VectorMap;
template <typename Key, typename Value>
struct VectorMapItem;
template <typename TagType, typename IDType = int32_t, IDType InvalidValue = -1>
struct StrongID
{
    IDType identifier;

    constexpr StrongID() : identifier(InvalidValue) {}

    explicit constexpr StrongID(IDType value) : identifier(value) {}

    [[nodiscard]] constexpr bool operator==(StrongID other) const { return identifier == other.identifier; }

    [[nodiscard]] constexpr bool operator!=(StrongID other) const { return identifier != other.identifier; }

    [[nodiscard]] constexpr bool isValid() const { return identifier != InvalidValue; }

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

template <typename Key, typename Value>
struct SC::VectorMapItem
{
    Key   key;
    Value value;
};

template <typename Key, typename Value, typename Container = SC::Vector<SC::VectorMapItem<Key, Value>>>
struct SC::VectorMap
{
    using Item = VectorMapItem<Key, Value>;

    Container items;

    [[nodiscard]] auto size() const { return items.size(); }
    [[nodiscard]] auto isEmpty() const { return items.isEmpty(); }

    [[nodiscard]] const Container& getItems() const { return items; }

    [[nodiscard]] Item*       begin() { return items.begin(); }
    [[nodiscard]] const Item* begin() const { return items.begin(); }
    [[nodiscard]] Item*       end() { return items.end(); }
    [[nodiscard]] const Item* end() const { return items.end(); }

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

    [[nodiscard]] Key* insertValueUniqueKey(Value&& value)
    {
        if (items.push_back({Key::generateUniqueKey(*this), forward<Value>(value)}))
        {
            return &items.back().key;
        }
        return nullptr;
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
    [[nodiscard]] const Value* get(const ComparableToKey& key) const
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                return &item.value;
            }
        }
        return nullptr;
    }

    template <typename ComparableToKey>
    [[nodiscard]] Value* get(const ComparableToKey& key)
    {
        for (auto& item : items)
        {
            if (item.key == key)
            {
                return &item.value;
            }
        }
        return nullptr;
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
