// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/AlignedStorage.h"
#include "../Foundation/InitializerList.h"
#include "../Foundation/TypeList.h"
namespace SC
{
template <typename Tag>
struct TaggedUnion;
template <typename EnumType, EnumType enumValue, typename MemberType>
struct TaggedType;
} // namespace SC

//! @addtogroup group_foundation_utility
//! @{

/// @brief Associate a Type to an Enum, as required by TaggedUnion
/// @tparam EnumType Type of the enumeration
/// @tparam MemberType Type of member of the union
/// @tparam enumValue Enumeration value that represents the Type
template <typename EnumType, EnumType enumValue, typename MemberType>
struct SC::TaggedType
{
    using type     = MemberType;
    using enumType = EnumType;

    static constexpr EnumType value = enumValue;
};

/// @brief Type safe union with an enum type, where each type has an associated enum value.
/// @tparam Union with `FieldTypes` = `TypeList<TaggedType<EnumType, EnumValue, Type>, ...>`
///
/// Example:
/// \snippet Libraries/Foundation/Tests/TaggedUnionTest.cpp TaggedUnionTestSnippet
template <typename Union>
struct SC::TaggedUnion
{
  private:
    template <int Index>
    using TypeAt = TypeTraits::TypeListGetT<typename Union::FieldsTypes, Index>;

  public:
    static constexpr auto NumTypes = Union::FieldsTypes::size;

    TaggedUnion()
    {
        type = TypeAt<0>::value;
        new (&fieldAt<0>(), PlacementNew()) typename TypeAt<0>::type();
    }

    /// @brief Destroys the TaggedUnion object
    ~TaggedUnion() { visit(Destruct()); }

    /// @brief Copy constructor
    /// @param other Another tagged union
    TaggedUnion(const TaggedUnion& other)
    {
        type = other.type;
        visit(CopyConstruct(), other);
    }

    /// @brief Move constructor
    /// @param other Another tagged union
    TaggedUnion(TaggedUnion&& other)
    {
        type = other.type;
        visit(MoveConstruct(), other);
    }

    /// @brief Copy assignment operator
    /// @param other Another tagged union
    TaggedUnion& operator=(const TaggedUnion& other)
    {
        if (type == other.type)
        {
            visit<CopyAssign>(other);
        }
        else
        {
            visit(Destruct());
            type = other.type;
            visit(CopyConstruct(), other);
        }
        return *this;
    }

    /// @brief Move assignment operator
    /// @param other Another tagged union
    TaggedUnion& operator=(TaggedUnion&& other)
    {
        if (type == other.type)
        {
            visit(MoveAssign(), other);
        }
        else
        {
            visit(Destruct());
            type = other.type;
            visit(MoveConstruct(), other);
        }
        return *this;
    }

    using EnumType = typename TypeAt<0>::enumType;

    /// @brief Extracts type `T` corresponding to enumeration wantedEnum at compile time
    template <EnumType wantedEnum, int StartIndex = NumTypes>
    struct EnumToType
    {
        static constexpr int index = wantedEnum == TypeAt<StartIndex - 1>::value
                                         ? StartIndex - 1
                                         : EnumToType<wantedEnum, StartIndex - 1>::index;
        static_assert(index >= 0, "Type not found!");
        using type = TypeTraits::ConditionalT<wantedEnum == TypeAt<StartIndex - 1>::value,            // Condition
                                              typename TypeAt<StartIndex - 1>::type,                  // True
                                              typename EnumToType<wantedEnum, StartIndex - 1>::type>; // False
    };

    template <EnumType wantedEnum>
    struct EnumToType<wantedEnum, 0>
    {
        using type                 = typename TypeAt<0>::type;
        static constexpr int index = 0;
    };

    /// @brief Returns enumeration value of currently active union type
    EnumType getType() const { return type; }

    /// @brief Sets the currently active type at runtime, destructing and (default) constructing the new type
    void setType(EnumType newType)
    {
        if (newType != type)
        {
            visit(Destruct());
            type = newType;
            visit(Construct());
        }
    }

    bool operator==(const TaggedUnion& other) const { return type == other.type and visit<Equals>(other); }

    /// @brief Assigns a compile time known enum type with an object U
    /// @tparam U Type of object to be assigned.
    /// @tparam wantedType Compile time known enum type
    /// @param other  object to be assigned (move / copy)
    template <EnumType wantedType, typename U>
    void assign(U&& other)
    {
        auto& field = fieldAt<EnumToType<wantedType>::index>();
        if (type == wantedType)
        {
            field = forward<U>(other);
        }
        else
        {
            using T = typename EnumToType<wantedType>::type;
            visit(Destruct());
            type = wantedType;
            new (&field, PlacementNew()) T(forward<U>(other));
        }
    }

    /// @brief Changes current active type in union to a different one.
    /// @tparam wantedType Compile time know enum value associated to wanted type
    /// @return A reference to the wanted type.
    /// If wantedType is not the active type, a new value will be default initialized
    /// @note Return type is made explicit instead of using `auto` to help intellisense deducing type
    template <EnumType wantedType>
    typename TypeAt<EnumToType<wantedType>::index>::type& changeTo()
    {
        using T     = typename EnumToType<wantedType>::type;
        auto& field = fieldAt<EnumToType<wantedType>::index>();
        if (type != wantedType)
        {
            visit(Destruct());
            type = wantedType;
            new (&field, PlacementNew()) T();
        }
        return field;
    }

    /// @brief Get a pointer to currently active field
    /// @tparam wantedType Compile time know enum value associated to wanted type
    /// @return A pointer to currently active field or nullptr if wantedType doesn't match currently active type.
    /// @note Return type is made explicit instead of using `auto` to help intellisense deducing type
    template <EnumType wantedType>
    [[nodiscard]] typename TypeAt<EnumToType<wantedType>::index>::type* field()
    {
        if (wantedType == type)
        {
            return &fieldAt<EnumToType<wantedType>::index>();
        }
        return nullptr;
    }

    /// @brief Get a pointer to currently active field
    /// @tparam wantedType Compile time know enum value associated to wanted type
    /// @return A pointer to currently active field or nullptr if wantedType doesn't match currently active type.
    /// @note Return type is made explicit instead of using `auto` to help intellisense deducing type
    template <EnumType wantedType>
    [[nodiscard]] const typename TypeAt<EnumToType<wantedType>::index>::type* field() const
    {
        if (wantedType == type)
        {
            return &fieldAt<EnumToType<wantedType>::index>();
        }
        return nullptr;
    }

  private:
    struct Destruct
    {
        template <int Index>
        void operator()(TaggedUnion& t1)
        {
            using T = typename TypeAt<Index>::type;
            t1.fieldAt<Index>().~T();
        }
    };

    struct Construct
    {
        template <int Index>
        void operator()(TaggedUnion& t1)
        {
            using T = typename TypeAt<Index>::type;
            new (&t1.fieldAt<Index>(), PlacementNew()) T();
        }
    };

    struct CopyConstruct
    {
        template <int Index>
        void operator()(TaggedUnion& t1, const TaggedUnion& t2)
        {
            using T = typename TypeAt<Index>::type;
            new (&t1.fieldAt<Index>(), PlacementNew()) T(t2.fieldAt<Index>());
        }
    };

    struct MoveConstruct
    {
        template <int Index>
        void operator()(TaggedUnion& t1, TaggedUnion& t2)
        {
            using T = typename TypeAt<Index>::type;
            new (&t1.fieldAt<Index>(), PlacementNew()) T(move(t2.fieldAt<Index>()));
        }
    };

    struct CopyAssign
    {
        template <int Index>
        void operator()(TaggedUnion& t1, const TaggedUnion& t2)
        {
            t1.fieldAt<Index>() = t2.fieldAt<Index>();
        }
    };

    struct MoveAssign
    {
        template <int Index>
        void operator()(TaggedUnion& t1, TaggedUnion& t2)
        {
            t1.fieldAt<Index>() = move(t2.fieldAt<Index>());
        }
    };

    struct Equals
    {
        template <int Index>
        static auto visit(TaggedUnion& t1, TaggedUnion& t2)
        {
            return t1.fieldAt<Index>() == move(t2.fieldAt<Index>());
        }
    };

    template <typename Visitor, typename... Arguments>
    auto visit(Visitor&& visitor, Arguments&&... args)
    {
        return RuntimeEnumVisit<Visitor>::visit(forward<Visitor>(visitor), type, *this, forward<Arguments>(args)...);
    }

    template <typename Visitor, size_t StartIndex = NumTypes>
    struct RuntimeEnumVisit
    {
        static constexpr auto Index = StartIndex - 1;

        template <typename... Args>
        static auto visit(Visitor&& visitor, EnumType enumType, Args&... args)
        {
            if (enumType == TypeAt<Index>::value)
            {
                return visitor.template operator()<Index>(args...);
            }
            else
            {
                return RuntimeEnumVisit<Visitor, Index>::visit(forward<Visitor>(visitor), enumType, args...);
            }
        }
    };

    template <typename Visitor>
    struct RuntimeEnumVisit<Visitor, 0>
    {
        template <typename... Args>
        static auto visit(Visitor&& visitor, EnumType, Args&... args)
        {
            return visitor.template operator()<0>(args...);
        }
    };

    template <int index>
    [[nodiscard]] auto& fieldAt()
    {
        using T = typename TypeAt<index>::type;
        return storage.template reinterpret_as<T>();
    }

    template <int index>
    [[nodiscard]] const auto& fieldAt() const
    {
        using T = typename TypeAt<index>::type;
        return storage.template reinterpret_as<const T>();
    }

    template <class ForwardIt>
    static constexpr ForwardIt MaxElement(ForwardIt first, ForwardIt last)
    {
        if (first == last)
            return last;
        ForwardIt largest = first;
        for (++first; first != last; ++first)
        {
            if (*largest < *first)
                largest = first;
        }

        return largest;
    }

    template <class T>
    static constexpr T MaxElement(std::initializer_list<T> list)
    {
        return *MaxElement(list.begin(), list.end());
    }

    template <class... Types>
    struct ComputeMaxSizeAndAlignment;

    template <class... Types>
    struct ComputeMaxSizeAndAlignment<TypeTraits::TypeList<Types...>>
    {
        static constexpr size_t maxAlignment = MaxElement({alignof(typename Types::type)...});
        static constexpr size_t maxSize      = MaxElement({sizeof(typename Types::type)...});
    };

    using Storage = ComputeMaxSizeAndAlignment<typename Union::FieldsTypes>;

    AlignedStorage<Storage::maxSize, Storage::maxAlignment> storage;

    EnumType type;
};
//! @}
