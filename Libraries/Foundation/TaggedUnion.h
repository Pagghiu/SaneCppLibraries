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
/**
 * @code{.cpp}
    // ... Declaration
    enum TestType
    {
        TypeString = 10,
        TypeInt    = 110,
    };

    // Associate each enumeration value with some Type
    struct TestUnion
    {
        template <TestType E, typename T>
        using Tag = TaggedType<TestType, E, T>; // Helper to save some typing

        using FieldsTypes = TypeTraits::TypeList
 *          <
                Tag<TypeString, String>,    // Associate String to TypeString
                Tag<TypeInt, int>           // Associate int to TypeInt
 *          >;
    };

    // ... Usage
    TaggedUnion<TestUnion> test; // default initialized to first type (String)

    // Access / Change type
    String* ptr = test.field<TypeString>();
    if(ptr) // If TypeString is not active type, ptr will be == nullptr
    {
        *ptr = "SomeValue";
    }
    test.changeTo<TypeInt>() = 2; // Change active type to TypeInt

    // Switch on currently active type
    switch(test.getType())
    {
        case TestString: console.print("String = {}", *test.field<TestString>()); break;
        case TestInt:    console.print("Int = {}",    *test.field<TestInt>());    break;
    }
 * @endcode
*/
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
    ~TaggedUnion() { destruct(); }

    /// @brief Copy constructor
    /// @param other Another tagged union
    TaggedUnion(const TaggedUnion& other)
    {
        type = other.type;
        RuntimeEnumVisit<CopyConstruct, TaggedUnion, const TaggedUnion>::visit(this, &other, type);
    }

    /// @brief Move constructor
    /// @param other Another tagged union
    TaggedUnion(TaggedUnion&& other)
    {
        type = other.type;
        RuntimeEnumVisit<MoveConstruct, TaggedUnion, TaggedUnion>::visit(this, &other, type);
    }

    /// @brief Copy assignment operator
    /// @param other Another tagged union
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

    /// @brief Move assignment operator
    /// @param other Another tagged union
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

    bool operator==(const TaggedUnion& other) const
    {
        return type == other.type and RuntimeEnumVisit<Equals, TaggedUnion, TaggedUnion>::visit(this, &other, type);
    }

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
            destruct();
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
            destruct();
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
