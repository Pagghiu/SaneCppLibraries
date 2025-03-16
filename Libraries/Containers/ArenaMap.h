// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "../Foundation/Memory.h"
#include "ArenaMapKey.h"

namespace SC
{
template <typename T>
struct ArenaMap;
} // namespace SC

//! @addtogroup group_containers
//! @{

/// @brief A sparse vector keeping objects at a stable memory location
/// @tparam T Type of items kept in this Arena
///
/// SC::ArenaMap is a container used to keep objects memory location stable. @n
/// Internally it hold sparse objects inside of a SC::Vector and for this reason it can only be SC::ArenaMap::resize-d
/// when it's empty.
/// @n Objects can be inserted up to the SC::ArenaMap::size and insertion returns *handle* keys allowing to retrieve the
/// inserted object in constant time.
///
/// \snippet Libraries/Containers/Tests/ArenaMapTest.cpp ArenaMapSnippet
template <typename T>
struct SC::ArenaMap
{
    using Key = ArenaMapKey<T>;

    ArenaMap() {}

    ~ArenaMap() { clear(); }

    ArenaMap(const ArenaMap& other) { *this = other; }

    ArenaMap(ArenaMap&& other) { *this = move(other); }

    ArenaMap& operator=(const ArenaMap& other)
    {
        clear();
        T* newItems = reinterpret_cast<T*>(Memory::allocate(other.itemsSize * sizeof(T)));

        typename Key::Generation* newGenerations = reinterpret_cast<typename Key::Generation*>(
            Memory::allocate(other.itemsSize * sizeof(typename Key::Generation)));
        SC_ASSERT_RELEASE(newItems);
        SC_ASSERT_RELEASE(newGenerations);
        ::memset(newGenerations, 0, other.itemsSize * sizeof(typename Key::Generation));
        items       = newItems;
        generations = newGenerations;
        itemsSize   = other.itemsSize;
        for (size_t idx = 0; idx < other.itemsSize; ++idx)
        {
            if (other.generations[idx].used)
            {
                new (&items[idx], PlacementNew()) T(other.items[idx]);
            }
            generations[idx] = other.generations[idx];
        }
        numUsed = other.numUsed;
        return *this;
    }

    ArenaMap& operator=(ArenaMap&& other)
    {
        clear();
        items             = other.items;
        itemsSize         = other.itemsSize;
        other.items       = nullptr;
        other.itemsSize   = 0;
        generations       = other.generations;
        other.generations = nullptr;
        numUsed           = other.numUsed;
        other.numUsed     = 0;
        return *this;
    }

    /// @brief Get the maximum number of objects that can be stored in this map
    uint32_t getNumAllocated() const { return static_cast<uint32_t>(itemsSize); }

    template <typename MapType>
    struct ArenaMapIterator
    {
        MapType* map   = nullptr;
        uint32_t index = 0;

        void operator++()
        {
            const auto numAllocated = map->getNumAllocated();
            for (++index; index < numAllocated; ++index)
            {
                if (map->generations[index].used)
                {
                    break;
                }
            }
        }

        bool operator==(ArenaMapIterator it) const
        {
            SC_ASSERT_DEBUG(it.map == map and map != nullptr);
            return it.index == index;
        }
        bool operator!=(ArenaMapIterator it) const
        {
            SC_ASSERT_DEBUG(it.map == map and map != nullptr);
            return it.index != index;
        }

        auto& operator*() const { return map->items[index]; }
        auto* operator->() const { return &map->items[index]; }
    };
    using ConstIterator = ArenaMapIterator<const ArenaMap>;
    using Iterator      = ArenaMapIterator<ArenaMap>;

    ConstIterator cbegin() const { return begin(); }
    ConstIterator cend() const { return end(); }
    ConstIterator begin() const
    {
        for (size_t idx = 0; idx < getNumAllocated(); ++idx)
        {
            if (generations[idx].used)
            {
                return {this, static_cast<uint32_t>(idx)};
            }
        }
        return end();
    }

    ConstIterator end() const { return {this, getNumAllocated()}; }

    Iterator begin()
    {
        for (size_t idx = 0; idx < getNumAllocated(); ++idx)
        {
            if (generations[idx].used)
            {
                return {this, static_cast<uint32_t>(idx)};
            }
        }
        return end();
    }

    Iterator end() { return {this, getNumAllocated()}; }

    void clear()
    {
        for (size_t idx = 0; idx < itemsSize; ++idx)
        {
            if (generations[idx].used)
            {
                items[idx].~T();
            }
            generations[idx].used = 0;
        }
        if (items)
            Memory::release(items);
        items     = nullptr;
        itemsSize = 0;
        if (generations)
            Memory::release(generations);
        generations = nullptr;
        numUsed     = 0;
    }

    /// @brief Get the number of used slots in the arena
    [[nodiscard]] size_t size() const { return numUsed; }

    /// @brief Get the total number slots in the arena
    [[nodiscard]] size_t capacity() const { return itemsSize; }

    /// @brief Returns true if size() == capacity(), that means the arena is full
    [[nodiscard]] bool isFull() const { return itemsSize == numUsed; }

    /// @brief Changes the size of the arena.
    /// @note Can only be called on empty arena (SC::ArenaMap::size == 0)
    /// @param newSize The new wanted number of elements to be stored in the arena
    /// @return `true` if resize succeeded
    [[nodiscard]] bool resize(size_t newSize)
    {
        if (numUsed != 0)
            return false;
        if (newSize > Key::MaxIndex)
            return false;
        if (items)
            Memory::release(items);
        items = nullptr;
        if (generations)
            Memory::release(generations);
        generations = nullptr;
        T* newItems = reinterpret_cast<T*>(Memory::allocate(newSize * sizeof(T)));
        if (not newItems)
            return false;
        items = newItems;
        typename Key::Generation* newGenerations =
            reinterpret_cast<typename Key::Generation*>(Memory::allocate(newSize * sizeof(typename Key::Generation)));
        if (not newGenerations)
            return false;
        ::memset(newGenerations, 0, newSize * sizeof(typename Key::Generation));
        generations = newGenerations;
        itemsSize   = newSize;
        numUsed     = 0;
        return true;
    }

    template <typename Value>
    [[nodiscard]] Key insert(const Value& object)
    {
        Key key = allocateNewKeySlot();
        if (key.isValid())
        {
            placementNew(items[key.index], object);
        }
        return key;
    }

    [[nodiscard]] Key allocate()
    {
        Key key = allocateNewKeySlot();
        if (key.isValid())
        {
            placementNew(items[key.index]);
        }
        return key;
    }

    template <typename Value>
    [[nodiscard]] Key insert(Value&& object)
    {
        Key key = allocateNewKeySlot();
        if (key.isValid())
        {
            placementNew(items[key.index], move(object));
        }
        return key;
    }

    [[nodiscard]] bool containsKey(Key key) const
    {
        return key.isValid() and generations[key.index].used != 0 and
               generations[key.index].generation == key.generation.generation;
    }

    template <typename ComparableToValue>
    [[nodiscard]] bool containsValue(const ComparableToValue& value, Key* optionalKey = nullptr) const
    {
        for (size_t idx = 0; idx < itemsSize; ++idx)
        {
            if (generations[idx].used and items[idx] == value)
            {
                if (optionalKey)
                {
                    optionalKey->index      = static_cast<uint32_t>(idx);
                    optionalKey->generation = generations[idx];
                }
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool remove(Key key)
    {
        if (generations[key.index] != key.generation)
            return false;
        if (generations[key.index].generation + 1 <= Key::MaxGenerations)
        {
            generations[key.index].generation++;
        }
        else
        {
            // TODO: Should we give option of failing on generation overflow?
            generations[key.index].generation++;
        }
        generations[key.index].used = 0;
        items[key.index].~T();
        numUsed--;
        return true;
    }

    [[nodiscard]] T* get(Key key)
    {
        if (generations[key.index] != key.generation)
            return nullptr;
        return &items[key.index];
    }

    [[nodiscard]] const T* get(Key key) const
    {
        if (generations[key.index] != key.generation)
            return nullptr;
        return &items[key.index];
    }

  private:
    T*     items     = nullptr;
    size_t itemsSize = 0;

    typename Key::Generation* generations = nullptr;

    uint32_t numUsed = 0;

    [[nodiscard]] Key allocateNewKeySlot()
    {
        for (size_t idx = 0; idx < itemsSize; ++idx)
        {
            if (generations[idx].used == 0)
            {
                generations[idx].used = 1;

                Key key;
                key.generation = generations[idx];
                key.index      = static_cast<uint32_t>(idx);
                numUsed++;
                return key;
            }
        }
        return {};
    }
};
//! @}
