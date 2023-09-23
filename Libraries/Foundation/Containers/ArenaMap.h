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

template <typename T>
struct SC::ArenaMapKey
{
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
        ArenaMap<T>::clear();
        SC_DEBUG_ASSERT(ArenaMap<T>::items.resize(other.size()));
        for (size_t idx = 0; idx < other.items.size(); ++idx)
        {
            if (other.generations[idx].used)
            {
                new (&ArenaMap<T>::items[idx].object, PlacementNew()) T(other.items[idx].object);
            }
        }
        ArenaMap<T>::generations = other.generations;
        ArenaMap<T>::numUsed     = other.numUsed;
        return *this;
    }

    ArenaMap& operator=(ArenaMap&& other)
    {
        ArenaMap<T>::clear();
        ArenaMap<T>::items       = move(other.items);
        ArenaMap<T>::generations = move(other.generations);
        ArenaMap<T>::numUsed     = other.numUsed;
        other.numUsed            = 0;
        return *this;
    }

    uint32_t getNumAllocated() const { return static_cast<uint32_t>(items.size()); }

    template <typename MapType>
    struct GenericIterator
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

        bool operator==(GenericIterator it) const
        {
            SC_DEBUG_ASSERT(it.map == map and map != nullptr);
            return it.index == index;
        }
        bool operator!=(GenericIterator it) const
        {
            SC_DEBUG_ASSERT(it.map == map and map != nullptr);
            return it.index != index;
        }

        auto& operator*() const { return map->items[index].object; }
        auto* operator->() const { return &map->items[index].object; }
    };
    using ConstIterator = GenericIterator<const ArenaMap>;
    using Iterator      = GenericIterator<ArenaMap>;

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

    [[nodiscard]] size_t size() const { return numUsed; }

    [[nodiscard]] bool resize(size_t newSize)
    {
        if (numUsed != 0)
            return false;
        if (newSize > Key::MaxIndex)
            return false;
        SC_TRY(items.resize(newSize));
        SC_TRY(generations.resize(newSize));
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

  protected:
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
