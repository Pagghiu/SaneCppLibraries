// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Language.h"

namespace SC
{
template <typename Value>
struct Optional;
} // namespace SC

template <typename Value>
struct [[nodiscard]] SC::Optional
{
    // We cannot have a reference in union, so we use ReferenceWrapper
    typedef typename Conditional<IsReference<Value>::value, ReferenceWrapper<Value>, Value>::type ValueType;

  private:
    union
    {
        ValueType value;
    };
    bool hasValue;

  public:
    constexpr operator bool() const { return hasValue; }

    SC_CONSTEXPR_CONSTRUCTOR_NEW Optional(const Value& v)
    {
        new (&value, PlacementNew()) ValueType(v);
        hasValue = true;
    }

    SC_CONSTEXPR_CONSTRUCTOR_NEW Optional(Value&& v)
    {
        new (&value, PlacementNew()) ValueType(forward<Value>(v));
        hasValue = true;
    }

    SC_CONSTEXPR_CONSTRUCTOR_NEW Optional() { hasValue = false; }

    SC_CONSTEXPR_DESTRUCTOR ~Optional()
    {
        if (hasValue)
        {
            value.~ValueType();
        }
    }

    SC_CONSTEXPR_CONSTRUCTOR_NEW Optional(Optional&& other) noexcept
    {
        hasValue = other.hasValue;
        if (hasValue)
        {
            new (&value, PlacementNew()) ValueType(move(other.value));
            other.hasValue = false;
        }
    }

    SC_CONSTEXPR_CONSTRUCTOR_NEW Optional(const Optional& other)
    {
        hasValue = other.hasValue;
        if (hasValue)
        {
            new (&value, PlacementNew()) ValueType(other.value);
        }
    }

    constexpr Optional& operator=(const Optional& other) const
    {
        if (hasValue)
        {
            value.~ValueType();
        }
        hasValue = other.hasValue;
        if (hasValue)
        {
            new (&value, PlacementNew()) ValueType(other.value);
        }
        return *this;
    }

    constexpr Optional& operator=(Optional&& other)
    {
        if (hasValue)
        {
            value.~ValueType();
        }
        hasValue = other.hasValue;
        if (hasValue)
        {
            new (&value, PlacementNew()) ValueType(move(other.value));
            other.hasValue = false;
        }
        return *this;
    }

    [[nodiscard]] bool moveTo(Value& destination)
    {
        if (hasValue)
        {
            destination = move(value);
            hasValue    = false;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool get(const Value*& pValue) const
    {
        if (hasValue)
        {
            pValue = &value;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool get(Value*& pValue) const
    {
        if (hasValue)
        {
            pValue = &value;
            return true;
        }
        return false;
    }
};
