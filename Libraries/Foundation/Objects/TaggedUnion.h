// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Algorithms/AlgorithmMinMax.h"
#include "../Objects/TypeList.h"

namespace SC
{
template <typename Tag>
struct TaggedUnion;
} // namespace SC

namespace SC
{
template <size_t Len, class... Types>
struct AlignedStorage
{
    static constexpr size_t alignment_value = max({alignof(Types)...});

    struct type
    {
        alignas(alignment_value) char _s[max({Len, sizeof(Types)...})];
    };
};

template <size_t Len, class... Types>
struct AlignedStorage<Len, TypeList<Types...>>
{
    static constexpr size_t alignment_value = max({alignof(typename Types::type)...});

    struct type
    {
        alignas(alignment_value) char _s[max({Len, sizeof(typename Types::type)...})];
    };
};

template <typename EnumType, EnumType enumValue, typename MemberType>
struct TaggedField
{
    using type                      = MemberType;
    using enumType                  = EnumType;
    static constexpr EnumType value = enumValue;
};

} // namespace SC

template <typename Union>
struct SC::TaggedUnion
{
  private:
    template <int Index>
    using TypeAt = TypeListGetT<typename Union::FieldsTypes, Index>;

  public:
    using EnumType                 = typename TypeAt<0>::enumType;
    static constexpr auto NumTypes = Union::FieldsTypes::size;
    template <typename T, int StartIndex = NumTypes>
    struct TypeToEnum
    {
      private:
        static constexpr auto CurrentIndex = StartIndex - 1;
        static_assert(CurrentIndex >= 0, "Type not found!");
        using CurrentType                  = typename TypeAt<CurrentIndex>::type;
        static constexpr auto CurrentValue = TypeAt<CurrentIndex>::value;
        static constexpr bool TypeFound    = IsSame<T, CurrentType>::value;

      public:
        static constexpr EnumType enumType = TypeFound ? CurrentValue : TypeToEnum<T, StartIndex - 1>::enumType;
        using type                 = ConditionalT<TypeFound, CurrentType, typename TypeToEnum<T, StartIndex - 1>::type>;
        static constexpr int index = TypeFound ? CurrentIndex : TypeToEnum<T, StartIndex - 1>::index;
    };

    template <typename T>
    struct TypeToEnum<T, 0>
    {
        using type = typename TypeAt<0>::type;
        static_assert(IsSame<T, type>::value, "Type doesn't exist in list of types");
        static constexpr int      index    = 0;
        static constexpr EnumType enumType = TypeAt<0>::value;
    };

    template <EnumType wantedEnum, int StartIndex = NumTypes>
    struct EnumToType
    {
        static constexpr int index = wantedEnum == TypeAt<StartIndex - 1>::value
                                         ? StartIndex - 1
                                         : EnumToType<wantedEnum, StartIndex - 1>::index;
        static_assert(index >= 0, "Type not found!");
        using type = ConditionalT<wantedEnum == TypeAt<StartIndex - 1>::value, typename TypeAt<StartIndex - 1>::type,
                                  typename EnumToType<wantedEnum, StartIndex - 1>::type>;
    };

    template <EnumType wantedEnum>
    struct EnumToType<wantedEnum, 0>
    {
        using type                 = typename TypeAt<0>::type;
        static constexpr int index = 0;
    };

  private:
    struct Destruct
    {
        template <int Index>
        static void visit(TaggedUnion* t1, TaggedUnion* t2)
        {
            SC_COMPILER_UNUSED(t2);
            using T = typename TypeAt<Index>::type;
            t1->fieldAt<Index>().~T();
        }
    };

    struct CopyConstruct
    {
        template <int Index>
        static void visit(TaggedUnion* t1, const TaggedUnion* t2)
        {
            using T = typename TypeAt<Index>::type;
            new (&t1->fieldAt<Index>(), PlacementNew()) T(t2->fieldAt<Index>());
        }
    };

    struct MoveConstruct
    {
        template <int Index>
        static void visit(TaggedUnion* t1, TaggedUnion* t2)
        {
            using T = typename TypeAt<Index>::type;
            new (&t1->fieldAt<Index>(), PlacementNew()) T(move(t2->fieldAt<Index>()));
        }
    };
    struct CopyAssign
    {
        template <int Index, typename T>
        static void visit(TaggedUnion* t1, const TaggedUnion* t2)
        {
            t1->fieldAt<Index>() = t2->fieldAt<Index>();
        }
    };
    struct MoveAssign
    {
        template <int Index>
        static void visit(TaggedUnion* t1, TaggedUnion* t2)
        {
            t1->fieldAt<Index>() = move(t2->fieldAt<Index>());
        }
    };

    struct Equals
    {
        template <int Index>
        static auto visit(TaggedUnion* t1, TaggedUnion* t2)
        {
            return t1->fieldAt<Index>() == move(t2->fieldAt<Index>());
        }
    };

    template <typename Visitor, typename T1, typename T2, size_t StartIndex = NumTypes>
    struct RuntimeEnumVisit
    {
        static constexpr auto Index = StartIndex - 1;

        static auto visit(T1* t1, T2* t2, EnumType enumType)
        {
            if (enumType == TypeAt<Index>::value)
            {
                return Visitor::template visit<Index>(t1, t2);
            }
            else
            {
                return RuntimeEnumVisit<Visitor, T1, T2, StartIndex - 1>::visit(t1, t2, enumType);
            }
        }
    };

    template <typename Visitor, typename T1, typename T2>
    struct RuntimeEnumVisit<Visitor, T1, T2, 0>
    {
        static auto visit(T1* t1, T2* t2, EnumType) { return Visitor::template visit<0>(t1, t2); }
    };

    void destruct() { RuntimeEnumVisit<Destruct, TaggedUnion, TaggedUnion>::visit(this, this, type); }

    template <int index>
    [[nodiscard]] auto& fieldAt()
    {
        using T = typename TypeAt<index>::type;
        return *reinterpret_cast<T*>(&storage);
    }

    template <int index>
    [[nodiscard]] const auto& fieldAt() const
    {
        using T = typename TypeAt<index>::type;
        return *reinterpret_cast<const T*>(&storage);
    }
    using Storage = typename AlignedStorage<0, typename Union::FieldsTypes>::type;

    Storage  storage;
    EnumType type;

  public:
    EnumType getType() const { return type; }

    bool operator==(const TaggedUnion& other) const
    {
        return type == other.type and RuntimeEnumVisit<Equals, TaggedUnion, TaggedUnion>::visit(this, &other, type);
    }

    TaggedUnion()
    {
        type = TypeAt<0>::value;
        new (&fieldAt<0>(), PlacementNew()) typename TypeAt<0>::type();
    }

    template <EnumType wantedEnum>
    struct Constructor
    {
        typename EnumToType<wantedEnum>::type object;
    };

    template <EnumType wantedEnum>
    TaggedUnion(Constructor<wantedEnum>&& other)
    {
        type    = wantedEnum;
        using T = typename EnumToType<wantedEnum>::type;
        new (&fieldAt<EnumToType<wantedEnum>::index>(), PlacementNew()) T(forward<T>(other.object));
    }

    template <typename U>
    TaggedUnion(EnumType wantedEnum, U&& other)
    {
        using T = typename RemoveConst<typename RemoveReference<U>::type>::type;
        type    = wantedEnum;

        fieldAt<TypeToEnum<T>::index>() = other;
    }

    ~TaggedUnion() { destruct(); }

    TaggedUnion(const TaggedUnion& other)
    {
        type = other.type;
        RuntimeEnumVisit<CopyConstruct, TaggedUnion, const TaggedUnion>::visit(this, &other, type);
    }

    TaggedUnion(TaggedUnion&& other)
    {
        type = other.type;
        RuntimeEnumVisit<MoveConstruct, TaggedUnion, TaggedUnion>::visit(this, &other, type);
    }

    TaggedUnion& operator=(const TaggedUnion& other)
    {
        if (type == other.type)
        {
            RuntimeEnumVisit<CopyAssign, TaggedUnion, const TaggedUnion>::visit(this, &other, type);
        }
        else
        {
            destruct();
            type = other.type;
            RuntimeEnumVisit<CopyConstruct, TaggedUnion, const TaggedUnion>::visit(this, &other, type);
        }
        return *this;
    }

    TaggedUnion& operator=(TaggedUnion&& other)
    {
        if (type == other.type)
        {
            RuntimeEnumVisit<MoveAssign, TaggedUnion, TaggedUnion>::visit(this, &other, type);
        }
        else
        {
            destruct();
            type = other.type;
            RuntimeEnumVisit<MoveConstruct, TaggedUnion, TaggedUnion>::visit(this, &other, type);
        }
        return *this;
    }

    template <EnumType wantedType, typename U>
    void assign(U&& other)
    {
        using T     = typename EnumToType<wantedType>::type;
        auto& field = fieldAt<EnumToType<wantedType>::index>();
        if (type == wantedType)
        {
            field = forward<U>(other);
        }
        else
        {
            destruct();
            type = wantedType;
            new (&field, PlacementNew()) T(forward<U>(other));
        }
    }

    template <EnumType wantedType>
    typename TypeAt<EnumToType<wantedType>::index>::type& changeTo()
    {
        using T     = typename EnumToType<wantedType>::type;
        auto& field = fieldAt<EnumToType<wantedType>::index>();
        if (type == wantedType)
        {
            return field;
        }
        else
        {
            destruct();
            type = wantedType;
            new (&field, PlacementNew()) T();
            return field;
        }
    }

    template <EnumType wantedType>
    [[nodiscard]] typename TypeAt<EnumToType<wantedType>::index>::type* field()
    {
        if (wantedType == type)
        {
            return &fieldAt<EnumToType<wantedType>::index>();
        }
        return nullptr;
    }

    template <EnumType wantedType>
    [[nodiscard]] const typename TypeAt<EnumToType<wantedType>::index>::type* field() const
    {
        if (wantedType == type)
        {
            return &fieldAt<EnumToType<wantedType>::index>();
        }
        return nullptr;
    }
};
