// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/TypeList.h"
#include "../Foundation/Types.h"
namespace SC
{
template <typename Tag>
struct TaggedUnion;
} // namespace SC

namespace SC
{
template <typename UnionType, typename EnumType, typename MemberType, MemberType UnionType::*field, EnumType enumValue>
struct TaggedField
{
    using type                      = MemberType;
    using enumType                  = EnumType;
    static constexpr EnumType value = enumValue;
    static auto&              get(UnionType& u) { return u.*field; }
    static const auto&        get(const UnionType& u) { return u.*field; }
};
} // namespace SC

template <typename Union>
struct SC::TaggedUnion
{
  private:
    template <int Index>
    using TypeAt   = TypeListGetT<typename Union::FieldsTypes, Index>;
    using EnumType = typename TypeAt<0>::enumType;

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
        static const EnumType enumType = TypeAt<0>::value;
        using type                     = typename TypeAt<0>::type;
        static constexpr int index     = 0;
    };

    struct Destruct
    {
        template <int Index>
        static void visit(TaggedUnion* t1, TaggedUnion* t2)
        {
            SC_UNUSED(t2);
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

    template <typename Visitor, typename T1, typename T2, size_t StartIndex = NumTypes>
    struct RuntimeEnumVisit
    {
        static constexpr auto Index = StartIndex - 1;

        static void visit(T1* t1, T2* t2, EnumType enumType)
        {
            if (enumType == TypeAt<Index>::value)
            {
                using Type = typename TypeAt<Index>::type;
                Visitor::template visit<Index>(t1, t2);
            }
            else
            {
                RuntimeEnumVisit<Visitor, T1, T2, StartIndex - 1>::visit(t1, t2, enumType);
            }
        }
    };

    template <typename Visitor, typename T1, typename T2>
    struct RuntimeEnumVisit<Visitor, T1, T2, 0>
    {
        static void visit(T1* t1, T2* t2, EnumType) { Visitor::template visit<0>(t1, t2); }
    };

    void destruct() { RuntimeEnumVisit<Destruct, TaggedUnion, TaggedUnion>::visit(this, this, type); }

  public:
    Union    fields;
    EnumType type;

    TaggedUnion()
    {
        type = TypeAt<0>::value;
        new (&fieldAt<0>(), PlacementNew()) typename TypeAt<0>::type();
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

    template <typename U>
    void assignValue(U&& other)
    {
        using T = typename RemoveConst<typename RemoveReference<U>::type>::type;
        if (type == TypeToEnum<T>::enumType)
        {
            *unionAs<T>() = forward<U>(other);
        }
        else
        {
            destruct();
            type = TypeToEnum<T>::enumType;
            new (unionAs<T>(), PlacementNew()) T(forward<U>(other));
        }
    }

    template <int index>
    [[nodiscard]] auto& fieldAt()
    {
        return TypeAt<index>::get(fields);
    }

    template <int index>
    [[nodiscard]] const auto& fieldAt() const
    {
        return TypeAt<index>::get(fields);
    }

    template <typename T>
    [[nodiscard]] T* unionAs()
    {
        if (TypeToEnum<T>::enumType == type)
        {
            return &TypeAt<TypeToEnum<T>::index>::get(fields);
        }
        return nullptr;
    }

    template <typename T>
    [[nodiscard]] const T* unionAs() const
    {
        if (TypeToEnum<T>::enumType == type)
        {
            return &TypeAt<TypeToEnum<T>::index>::get(fields);
        }
        return nullptr;
    }
};
