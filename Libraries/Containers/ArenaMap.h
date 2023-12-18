// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Vector.h"

namespace SC
{

template <typename T>
struct ArenaMap;

template <typename T>
struct ArenaMapKey;
} // namespace SC

//! @addtogroup group_containers
//! @{

/// @brief A sparse vector keeping objects at a stable memory location.
///         All operations return an SC::ArenaMapKey that can be used to recover values in constant time.
/// @tparam T Type of items kept in this Arena
template <typename T>
struct SC::ArenaMapKey
{
  private:
    struct Generation
    {
        uint32_t used       : 1;
        uint32_t generation : 31;
        Generation()
        {
            used       = 0;
            generation = 0;
        }
        bool operator==(const Generation other) const { return used == other.used and generation == other.generation; }
        bool operator!=(const Generation other) const { return used != other.used or generation != other.generation; }
    };
    Generation generation;
    uint32_t   index;
    friend struct ArenaMap<T>;
    template <typename U>
    friend struct ArenaMapKey;

  public:
    ArenaMapKey() { index = 0; }

    bool isValid() const { return generation.used != 0; }

    template <typename U>
    ArenaMapKey<U> cast_to()
    {
        ArenaMapKey<U> key;
        key.generation.used       = generation.used;
        key.generation.generation = generation.generation;
        key.index                 = index;
        return key;
    }

    template <typename U>
    bool operator==(ArenaMapKey<U> other) const
    {
        return index == other.index and generation.used == other.generation.used and
               generation.generation == other.generation.generation;
    }

    static constexpr uint32_t MaxGenerations = (uint32_t(1) << 31) - 1;
    static constexpr uint32_t MaxIndex       = 0xffffffff;
};

/// @brief A sparse vector keeping objects at a stable memory location
/// @tparam T Type of items kept in this Arena
///
/// SC::ArenaMap is a container used to keep objects memory location stable. @n
/// Internally it hold sparse objects inside of a SC::Vector and for this reason it can only be SC::ArenaMap::resize-d
/// when it's empty.
/// @n Objects can be inserted up to the SC::ArenaMap::size and insertion returns *handle* keys allowing to retrieve the
/// inserted object in constant time.
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
        SC_ASSERT_RELEASE(items.resize(other.size()));
        for (size_t idx = 0; idx < other.items.size(); ++idx)
        {
            if (other.generations[idx].used)
            {
                new (&items[idx].object, PlacementNew()) T(other.items[idx].object);
            }
        }
        generations = other.generations;
        numUsed     = other.numUsed;
        return *this;
    }

    ArenaMap& operator=(ArenaMap&& other)
    {
        clear();
        items         = move(other.items);
        generations   = move(other.generations);
        numUsed       = other.numUsed;
        other.numUsed = 0;
        return *this;
    }

    /// @brief Get the maximum number of objects that can be stored in this map
    uint32_t getNumAllocated() const { return static_cast<uint32_t>(items.size()); }

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

        auto& operator*() const { return map->items[index].object; }
        auto* operator->() const { return &map->items[index].object; }
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
        for (size_t idx = 0; idx < items.size(); ++idx)
        {
            if (generations[idx].used)
            {
                items[idx].object.~T();
            }
        }
        generations.clear();
        items.clear();
        numUsed = 0;
    }

    /// @brief Get the number of used slots in the arena
    [[nodiscard]] size_t size() const { return numUsed; }

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
        if (not items.resize(newSize))
            return false;
        if (not generations.resize(newSize))
            return false;
        numUsed = 0;
        return true;
    }

    template <typename Value>
    [[nodiscard]] Key insert(const Value& object)
    {
        Key key = allocateNewKeySlot();
        if (key.isValid())
        {
            new (&items[key.index].object, PlacementNew()) T(object);
        }
        return key;
    }

    [[nodiscard]] Key allocate()
    {
        Key key = allocateNewKeySlot();
        if (key.isValid())
        {
            new (&items[key.index].object, PlacementNew()) T();
        }
        return key;
    }

    template <typename Value>
    [[nodiscard]] Key insert(Value&& object)
    {
        Key key = allocateNewKeySlot();
        if (key.isValid())
        {
            new (&items[key.index].object, PlacementNew()) T(move(object));
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
        for (size_t idx = 0; idx < items.size(); ++idx)
        {
            if (generations[idx].used and items[idx].object == value)
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
        items[key.index].object.~T();
        numUsed--;
        return true;
    }

    [[nodiscard]] T* get(Key key)
    {
        if (generations[key.index] != key.generation)
            return nullptr;
        return &items[key.index].object;
    }

    [[nodiscard]] const T* get(Key key) const
    {
        if (generations[key.index] != key.generation)
            return nullptr;
        return &items[key.index].object;
    }

  private:
    struct Item
    {
        T object;
        Item() {}
        Item(const Item&) {}
        Item& operator=(const Item&) {}
        ~Item() {}
    };

    Vector<Item> items;

    Vector<typename Key::Generation> generations;

    uint32_t numUsed = 0;

    [[nodiscard]] Key allocateNewKeySlot()
    {
        for (size_t idx = 0; idx < generations.size(); ++idx)
        {
            if (generations[idx].used == 0)
            {
                Key key;
                generations[idx].used = 1;
                key.generation        = generations[idx];
                key.index             = static_cast<uint32_t>(idx);
                numUsed++;
                return key;
            }
        }
        return {};
    }
};
//! @}
