// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Objects/MetaProgramming.h" // Conditional

namespace SC
{
template <typename Value>
struct Optional;

template <typename Value>
struct UniqueOptional;
} // namespace SC

template <typename Value>
struct [[nodiscard]] SC::Optional
{
    // We cannot have a reference in union, so we use ReferenceWrapper
    using ValueType = typename Conditional<IsReference<Value>::value, ReferenceWrapper<Value>, Value>::type;

  private:
    union
    {
        ValueType value;
    };
    bool valueExists;

  public:
    [[nodiscard]] bool hasValue() const { return valueExists; }
    constexpr Optional(const Value& v)
    {
        new (&value, PlacementNew()) ValueType(v);
        valueExists = true;
    }

    constexpr Optional(Value&& v)
    {
        new (&value, PlacementNew()) ValueType(forward<Value>(v));
        valueExists = true;
    }

    constexpr Optional() { valueExists = false; }

    SC_LANGUAGE_CONSTEXPR_DESTRUCTOR ~Optional()
    {
        if (valueExists)
        {
            value.~ValueType();
        }
    }

    constexpr Optional(Optional&& other) noexcept
    {
        valueExists = other.valueExists;
        if (valueExists)
        {
            new (&value, PlacementNew()) ValueType(move(other.value));
            other.valueExists = false;
        }
    }

    constexpr Optional(const Optional& other)
    {
        valueExists = other.valueExists;
        if (valueExists)
        {
            new (&value, PlacementNew()) ValueType(other.value);
        }
    }

    constexpr Optional& operator=(const Optional& other)
    {
        if (valueExists)
        {
            value.~ValueType();
        }
        valueExists = other.valueExists;
        if (valueExists)
        {
            new (&value, PlacementNew()) ValueType(other.value);
        }
        return *this;
    }

    constexpr Optional& operator=(Optional&& other)
    {
        if (valueExists)
        {
            value.~ValueType();
        }
        valueExists = other.valueExists;
        if (valueExists)
        {
            new (&value, PlacementNew()) ValueType(move(other.value));
            other.valueExists = false;
        }
        return *this;
    }

    [[nodiscard]] bool moveTo(Value& destination)
    {
        if (valueExists)
        {
            destination = move(value);
            valueExists = false;
            return true;
        }
        return false;
    }

    void clear()
    {
        if (valueExists)
        {
            value.~ValueType();
        }
        valueExists = false;
    }

    void assign(Value&& source)
    {
        if (valueExists)
        {
            value.~ValueType();
        }
        new (&value, PlacementNew()) ValueType(forward<Value>(source));
        valueExists = true;
    }

    [[nodiscard]] bool get(const Value*& pValue) const
    {
        if (valueExists)
        {
            pValue = &value;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool get(Value*& pValue)
    {
        if (valueExists)
        {
            pValue = &value;
            return true;
        }
        return false;
    }

    [[nodiscard]] Value* get()
    {
        if (valueExists)
            return &value;
        return nullptr;
    }

    [[nodiscard]] const Value* get() const
    {
        if (valueExists)
            return &value;
        return nullptr;
    }
};

template <typename Value>
struct [[nodiscard]] SC::UniqueOptional : public Optional<Value>
{
    UniqueOptional()                                 = default;
    ~UniqueOptional()                                = default;
    UniqueOptional(UniqueOptional&&)                 = default;
    UniqueOptional& operator=(UniqueOptional&&)      = default;
    UniqueOptional(const UniqueOptional&)            = delete;
    UniqueOptional& operator=(const UniqueOptional&) = delete;
};
