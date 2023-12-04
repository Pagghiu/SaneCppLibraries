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
struct StrongID;
} // namespace SC
//! @addtogroup group_containers
//! @{

/// @brief Strongly typed ID (cannot be assigned incorrectly to another ID)
/// @tparam TagType An empty class used just to tag this StrongID with a strong type
/// @tparam IDType The primitive type (typically `int` or similar) used to represent the ID
/// @tparam InvalidValue The sentinel primitive value that represents an invalid state of the ID
template <typename TagType, typename IDType, IDType InvalidValue>
struct SC::StrongID
{
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

/// @brief The single item of VectorMap, holding a Key and Value
/// @tparam Key The type representing Key in the map.
/// @tparam Value The type representing Value in the map.
template <typename Key, typename Value>
struct SC::VectorMapItem
{
    Key   key;   ///< Key item value
    Value value; ///< Map item value
};

/// @brief A simple map holding VectorMapItem key-value pairs in an unsorted Vector
/// @tparam Key Type of the key (must support `==` comparison)
/// @tparam Value Value type associated with Key
/// @tparam Container Container used for the Map
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

    /// @brief Remove an item with matching key from the Map
    /// @param key The key that must be removed
    /// @return `true` if the item was found
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
