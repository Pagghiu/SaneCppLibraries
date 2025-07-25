// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/StrongID.h"
#include "../Containers/Vector.h"

namespace SC
{
template <typename Key, typename Value, typename Container>
struct VectorMap;
template <typename Key, typename Value>
struct VectorMapItem;
} // namespace SC
//! @addtogroup group_containers
//! @{

/// @brief The single item of VectorMap, holding a Key and Value
/// @tparam Key The type representing Key in the map.
/// @tparam Value The type representing Value in the map.
template <typename Key, typename Value>
struct SC::VectorMapItem
{
    Key   key;   ///< Key item value
    Value value; ///< Map item value
};

/// @brief A map holding VectorMapItem key-value pairs in an unsorted Vector
/// @tparam Key Type of the key (must support `==` comparison)
/// @tparam Value Value type associated with Key
/// @tparam Container Container used for the Map
///
/// \snippet Tests/Libraries/Containers/VectorMapTest.cpp VectorMapSnippet
template <typename Key, typename Value, typename Container = SC::Vector<SC::VectorMapItem<Key, Value>>>
struct SC::VectorMap
{
    using Item = VectorMapItem<Key, Value>;

    Container items;

    /// @brief Return the number of key-value pairs in the map
    [[nodiscard]] auto size() const { return items.size(); }

    /// @brief Check if the map is empty
    [[nodiscard]] auto isEmpty() const { return items.isEmpty(); }

    [[nodiscard]] Item*       begin() { return items.begin(); }
    [[nodiscard]] const Item* begin() const { return items.begin(); }
    [[nodiscard]] Item*       end() { return items.end(); }
    [[nodiscard]] const Item* end() const { return items.end(); }

    /// @brief Remove an item with matching key from the Map
    /// @param key The key that must be removed
    /// @return `true` if the item was found
    template <typename ComparableToKey>
    [[nodiscard]] bool remove(const ComparableToKey& key)
    {
        size_t idx = 0;
        for (auto& item : items)
        {
            if (item.key == key)
            {
                return items.removeAt(idx);
            }
            ++idx;
        }
        return false;
    }

    /// @brief Inserts an item if it doesn't exist already.
    /// @param item The item to insert
    /// @return `false` if item already exists or if insertion fails (`true` otherwise)
    [[nodiscard]] bool insertIfNotExists(Item&& item)
    {
        if (not contains(item.key))
        {
            return items.push_back(forward<Item>(item));
        }
        return false;
    }

    /// @brief Insert an item, overwriting the potentially already existing one
    /// @param item Item to insert
    /// @return A pointer to the Value if insertion succeeds, `nullptr` if insertion fails.
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

    /// @brief Inserts a new value, automatically generating key with Key::generateUniqueKey (works for StrongID for
    /// example)
    /// @param value The new value to be inserted
    /// @return A pointer to the new Key or `nullptr` if the map is full
    [[nodiscard]] Key* insertValueUniqueKey(Value&& value)
    {
        if (items.push_back({Key::generateUniqueKey(*this), forward<Value>(value)}))
        {
            return &items.back().key;
        }
        return nullptr;
    }

    /// @brief Check if the given key is contained in the map
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

    /// @brief Check if the given key is contained in the map
    /// @param key The key to search for inside current map
    /// @param outValue A reference that will receive pointer to the found element (if found)
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

    /// @brief Check if the given key is contained in the map
    /// @param key The key to search for inside current map
    /// @param outValue A reference that will receive pointer to the found element (if found)
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

    /// @brief Get the Value associated to the given key
    /// @return A pointer to the value if it exists in the map, `nullptr` otherwise
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

    /// @brief Get the Value associated to the given key
    /// @return A pointer to the value if it exists in the map, `nullptr` otherwise
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

    /// @brief Get the value associated to the given key, or creates a new one if needed
    /// @return A pointer to the value or `nullptr` if the map is full
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
//! @}
