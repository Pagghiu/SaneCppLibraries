// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Containers/VectorMap.h"
namespace SC
{
template <typename Type, typename Union>
struct TaggedMap;
}
template <typename Type, typename Union>
struct SC::TaggedMap
{
    VectorMap<Type, Union> flags;

    template <Type enumType>
    [[nodiscard]] typename Union::template EnumToType<enumType>::type* getOrCreate()
    {
        auto val = flags.getOrCreate(enumType);
        if (val)
        {
            return &val->template changeTo<enumType>();
        }
        return nullptr;
    }

    template <Type enumType>
    [[nodiscard]] bool set(const typename Union::template EnumToType<enumType>::type& obj)
    {
        auto res = flags.getOrCreate(enumType);
        if (res)
        {
            res->template changeTo<enumType>() = obj;
            return true;
        }
        return false;
    }

    template <Type enumType>
    [[nodiscard]] const typename Union::template EnumToType<enumType>::type* get() const
    {
        auto res = flags.get(enumType);
        if (res)
        {
            return res->template field<enumType>();
        }
        return nullptr;
    }

    [[nodiscard]] bool clear(Type enumType) { return flags.remove(enumType); }

    template <Type enumType, typename U>
    [[nodiscard]] bool hasValue(const U& obj) const
    {
        const auto entry = flags.get(enumType);
        if (entry)
        {
            auto field = entry->template field<enumType>();
            if (field)
            {
                return *field == obj;
            }
        }
        return false;
    }
};
