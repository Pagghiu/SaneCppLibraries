// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Limits.h" // MaxValue
#include "ReflectionFoundation.h"

namespace SC
{
namespace Reflection
{
/// @brief Enumeration of possible category types recognized by Reflection.
/// @note We can use only 7 of the 8 bits here, as in TypeInfo we're stealing 1 bit for TypeInfo::hasLink
enum class TypeCategory : uint8_t
{
    TypeInvalid = 0, ///< Invalid type sentinel

    // Primitive types
    TypeUINT8    = 1,  ///< Type is `uint8_t`
    TypeUINT16   = 2,  ///< Type is `uint16_t`
    TypeUINT32   = 3,  ///< Type is `uint32_t`
    TypeUINT64   = 4,  ///< Type is `uint64_t`
    TypeINT8     = 5,  ///< Type is `int8_t`
    TypeINT16    = 6,  ///< Type is `int16_t`
    TypeINT32    = 7,  ///< Type is `int32_t`
    TypeINT64    = 8,  ///< Type is `int64_t`
    TypeFLOAT32  = 9,  ///< Type is `float`
    TypeDOUBLE64 = 10, ///< Type is `double`

    // Non primitive types
    TypeStruct = 11, ///< Type is a struct type
    TypeArray  = 12, ///< Type is an array type
    TypeVector = 13, ///< Type is a vector type
};

/// @brief A single 8 bytes union holding most important information about a reflected type
/// This structure is expected to be stored in an flat array.
/// Children items are always following parent items in the flat array.
/// For example when a struct is defined, its members are defined as its children.
/// When one of these children is simple primitive type, this type is stored inline with the type itself.
/// When instead a complex type is needed, a linkID is provided.
/// Such link is as offset in the flat array where detailed definition of the complex type exists.
struct TypeInfo
{
    bool         hasLink : 1; ///< Contains a link to another type
    TypeCategory type    : 7; ///< Type of typeinfo
    union
    {
        uint8_t numberOfChildren; ///< Only valid when TypeInfo::hasLink == `false`
        uint8_t linkIndex;        ///< Only valid when TypeInfo::hasLink == `true`
    };
    uint16_t sizeInBytes; ///< Size in bytes of the described type

    /// @brief Holds no extended type info
    struct EmptyInfo
    {
    };

    /// @brief Holds extended type info for members of struct
    struct MemberInfo
    {
        uint16_t order;
        uint16_t offsetInBytes;
        constexpr MemberInfo(uint8_t order, uint16_t offsetInBytes) : order(order), offsetInBytes(offsetInBytes) {}
    };

    /// @brief Holds extended type info for structs
    struct StructInfo
    {
        bool isPacked : 1; ///< Ensures no padding (recursively) for the entire span of the struct
        constexpr StructInfo() : isPacked(false) {}
    };

    /// @brief Holds extended type info for array-like types
    struct ArrayInfo
    {
        uint32_t numElements; ///< Number of elements in the array
        constexpr ArrayInfo(uint32_t numElements) : numElements(numElements) {}
    };
    union
    {
        EmptyInfo  emptyInfo;
        MemberInfo memberInfo;
        StructInfo structInfo;
        ArrayInfo  arrayInfo;
    };

    /// @brief Constructs an invalid type info.
    constexpr TypeInfo()
        : type(TypeCategory::TypeInvalid), hasLink(false), numberOfChildren(0), emptyInfo(), sizeInBytes(0)
    {
        static_assert(sizeof(TypeInfo) == 8, "Size must be 8 bytes");
    }

    /// @brief Constructs a TypeInfo used by Struct Types
    constexpr TypeInfo(TypeCategory type, uint16_t sizeInBytes, StructInfo structInfo)
        : type(type), hasLink(false), numberOfChildren(0), sizeInBytes(sizeInBytes), structInfo(structInfo)
    {}

    /// @brief Constructs a TypeInfo used by Struct Members (children of Struct Type)
    constexpr TypeInfo(TypeCategory type, uint16_t sizeInBytes, MemberInfo member)
        : type(type), hasLink(true), linkIndex(0), sizeInBytes(sizeInBytes), memberInfo(member)
    {}

    /// @brief Constructs a TypeInfo used  by Array-like Types (T[N], Array<T, N> and Vector<T>)
    constexpr TypeInfo(TypeCategory type, uint16_t sizeInBytes, uint8_t numberOfChildren, ArrayInfo arrayInfo)
        : type(type), hasLink(false), numberOfChildren(numberOfChildren), sizeInBytes(sizeInBytes), arrayInfo(arrayInfo)
    {}

    /// @brief Constructs a TypeInfo of given type and size
    constexpr TypeInfo(TypeCategory type, uint16_t sizeInBytes)
        : type(type), hasLink(true), linkIndex(0), sizeInBytes(sizeInBytes), emptyInfo()
    {}

    /// @brief Get number of children (if any) of this info. Only valid when `hasLink` == `false`
    [[nodiscard]] constexpr auto getNumberOfChildren() const { return numberOfChildren; }

    /// @brief Sets the number of children of this typeinfo
    /// @param numChildren New number of children
    /// @return `true` if it has been changed successfully
    [[nodiscard]] constexpr bool setNumberOfChildren(size_t numChildren)
    {
        if (numChildren > static_cast<decltype(numberOfChildren)>(MaxValue()))
            return false;
        numberOfChildren = static_cast<decltype(numberOfChildren)>(numChildren);
        return true;
    }

    /// @brief Check if this type info has a valid ilink index
    [[nodiscard]] constexpr bool hasValidLinkIndex() const { return hasLink and linkIndex > 0; }

    /// @brief Check if this type info needs to be linked
    [[nodiscard]] constexpr bool needsLinking() const { return hasLink and linkIndex == 0; }

    /// @brief Obtains link valid index (assuming `hasLink` == `true` and needsLinking() == `false`)
    /// @return The link index for this type that indicates the linked type
    [[nodiscard]] constexpr auto getLinkIndex() const { return linkIndex; }

    /// @brief Change Link index for this type
    /// @param newLinkIndex New Link Index
    /// @return `true` if the link index has been changed successfully
    [[nodiscard]] constexpr bool setLinkIndex(ssize_t newLinkIndex)
    {
        if (newLinkIndex > static_cast<decltype(linkIndex)>(MaxValue()))
            return false;
        linkIndex = static_cast<decltype(linkIndex)>(newLinkIndex);
        return true;
    }

    /// @brief Check if type is primitive
    [[nodiscard]] constexpr bool isPrimitiveType() const { return isPrimitiveCategory(type); }

    /// @brief Check if type is primitive or it's a struct with `isPacked` property == `true`
    [[nodiscard]] constexpr bool isPrimitiveOrPackedStruct() const
    {
        if (isPrimitiveType())
            return true;
        return (type == Reflection::TypeCategory::TypeStruct) and structInfo.isPacked;
    }

    [[nodiscard]] static constexpr bool isPrimitiveCategory(TypeCategory category)
    {
        return category >= TypeCategory::TypeUINT8 and category <= TypeCategory::TypeDOUBLE64;
    }
};

/// @brief Basic class template that must be partially specialized for each type.
/// @tparam T The type to specialize for
template <typename T>
struct Reflect;

/// @brief Class template used to check if a given type `IsPacked` property is `true` at compile time
template <typename T, typename SFINAESelector = void>
struct ExtendedTypeInfo;

/// @brief Holds together a TypeInfo, a StringView and a type-erased builder function pointer
/// @tparam TypeVisitor The type of member visitor that is parameter of the builder function
template <typename TypeVisitor>
struct ReflectedType
{
    using TypeBuildFunction = bool (*)(TypeVisitor& builder);

    TypeInfo          typeInfo;
    TypeStringView    typeName;
    TypeBuildFunction typeBuild;

    constexpr ReflectedType() : typeBuild(nullptr) {}
    constexpr ReflectedType(const TypeInfo typeInfo, TypeStringView typeName, TypeBuildFunction typeBuild)
        : typeInfo(typeInfo), typeName(typeName), typeBuild(typeBuild)
    {}

    /// @brief Create from a generic type T
    template <typename T>
    [[nodiscard]] static constexpr ReflectedType createGeneric()
    {
        return {TypeInfo(Reflect<T>::getCategory(), sizeof(T)), TypeToString<T>::get(), &Reflect<T>::build};
    }

    /// @brief Create from a Struct type T
    template <typename T>
    [[nodiscard]] static constexpr ReflectedType createStruct(TypeStringView name = TypeToString<T>::get())
    {
        TypeInfo::StructInfo structInfo;
        structInfo.isPacked = ExtendedTypeInfo<T>::IsPacked;
        return {TypeInfo(Reflect<T>::getCategory(), sizeof(T), structInfo), name, &Reflect<T>::build};
    }

    /// @brief Create from a struct member with given name, order and offset
    template <typename R, typename T, int N>
    [[nodiscard]] static constexpr ReflectedType createMember(uint8_t order, const char (&name)[N], R T::*,
                                                              size_t offset)
    {
        const auto info = TypeInfo::MemberInfo(order, static_cast<SC::uint16_t>(offset));
        return {TypeInfo(Reflect<R>::getCategory(), sizeof(R), info), TypeStringView(name, N), &Reflect<R>::build};
    }

    /// @brief Create from an array-like type
    template <typename T>
    [[nodiscard]] static constexpr ReflectedType createArray(TypeStringView name, uint8_t numChildren,
                                                             TypeInfo::ArrayInfo arrayInfo)
    {
        return {TypeInfo(Reflect<T>::getCategory(), sizeof(T), numChildren, arrayInfo), name, &Reflect<T>::build};
    }
};

//-----------------------------------------------------------------------------------------------------------
// Primitive Types Support
//-----------------------------------------------------------------------------------------------------------

/// @brief Base struct for all primitive types
struct PrimitiveType
{
    template <typename TypeVisitor>
    [[nodiscard]] static constexpr bool build(TypeVisitor&)
    {
        return true;
    }
};

// clang-format off
template <> struct Reflect<char>     : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeUINT8;}};
template <> struct Reflect<uint8_t>  : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeUINT8;}};
template <> struct Reflect<uint16_t> : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeUINT16;}};
template <> struct Reflect<uint32_t> : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeUINT32;}};
template <> struct Reflect<uint64_t> : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeUINT64;}};
template <> struct Reflect<int8_t>   : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeINT8;}};
template <> struct Reflect<int16_t>  : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeINT16;}};
template <> struct Reflect<int32_t>  : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeINT32;}};
template <> struct Reflect<int64_t>  : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeINT64;}};
template <> struct Reflect<float>    : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeFLOAT32;}};
template <> struct Reflect<double>   : public PrimitiveType {static constexpr TypeCategory getCategory(){return TypeCategory::TypeDOUBLE64;}};

/// @brief Checks if a given type is primitive
template <typename T> struct IsPrimitive { static constexpr bool value = TypeInfo::isPrimitiveCategory(Reflect<T>::getCategory()); };

// clang-format on

template <typename T>
struct ExtendedTypeInfo<T, typename SC::EnableIf<IsPrimitive<T>::value>::type>
{
    // Primitive types are packed
    static constexpr bool IsPacked = true;
};

//-----------------------------------------------------------------------------------------------------------
// Arrays Support
//-----------------------------------------------------------------------------------------------------------
template <typename T, size_t N>
struct Reflect<T[N]>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeArray; }

    template <typename TypeVisitor>
    [[nodiscard]] static constexpr bool build(TypeVisitor& builder)
    {
        using Type = typename TypeVisitor::Type;

        // Add array type
        if (not builder.addType(Type::template createArray<T[N]>("Array", 1, TypeInfo::ArrayInfo{N})))
            return false;

        // Add dependent item type
        if (not builder.addType(Type::template createGeneric<T>()))
            return false;

        return true;
    }
};

template <typename T, int N>
struct ExtendedTypeInfo<T[N]>
{
    // Arrays are packed if T is packed
    static constexpr bool IsPacked = ExtendedTypeInfo<T>::IsPacked;
};

//-----------------------------------------------------------------------------------------------------------
// Structs Support
//-----------------------------------------------------------------------------------------------------------
template <typename Type>
struct ReflectStruct
{
    using T = Type;

    [[nodiscard]] static constexpr TypeCategory getCategory() { return TypeCategory::TypeStruct; }

    template <typename TypeVisitor>
    [[nodiscard]] static constexpr bool build(TypeVisitor& builder)
    {
        if (not builder.addType(TypeVisitor::Type::template createStruct<T>()))
            return false;
        if (not Reflect<Type>::visit(builder))
            return false;
        return true;
    }

    template <typename TypeVisitor>
    static constexpr bool visitObject(TypeVisitor&& builder, Type& object)
    {
        return Reflect<Type>::visit(VisitObjectAdapter<TypeVisitor>{builder, object});
    }

  private:
    template <typename TypeVisitor>
    struct VisitObjectAdapter
    {
        TypeVisitor& builder;
        Type&        object;

        template <typename R, int N>
        constexpr bool operator()(int order, const char (&name)[N], R Type::*field, size_t offset) const
        {
            SC_COMPILER_UNUSED(offset);
            return builder(order, name, object.*field);
        }
    };
};

/// @brief Visit all struct members to gather sum of their sizes (helper for IsPacked).
template <typename T>
struct ExtendedStructTypeInfo
{
    size_t memberSizeSum = 0;
    bool   IsPacked      = false;

    constexpr ExtendedStructTypeInfo()
    {
        // Let's call operator() for each member
        if (Reflect<T>::visit(*this))
        {
            // Structs are packed if all of its members are packed (checked in `operator()`)
            // and in addition to that, summed size of all members is `==` `sizeof(T)`
            IsPacked = memberSizeSum == sizeof(T);
        }
    }

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset)
    {
        SC_COMPILER_UNUSED(order);
        SC_COMPILER_UNUSED(name);
        SC_COMPILER_UNUSED(member);
        SC_COMPILER_UNUSED(offset);
        if (not ExtendedTypeInfo<R>().IsPacked)
        {
            return false; // If a given type is not packed, let's stop iterating
        }
        memberSizeSum += sizeof(R);
        return true; // continue iterating
    }
};

template <typename T, typename SFINAESelector>
struct ExtendedTypeInfo
{
    // Struct is packed if all of its members are packed and sum of their size equals size of struct.
    static constexpr bool IsPacked = ExtendedStructTypeInfo<T>().IsPacked;
};
} // namespace Reflection
} // namespace SC

// Handy Macros to avoid some typing when wrapping structs

/// @brief Start describing all members of a structure
#define SC_META_STRUCT_VISIT(StructName)                                                                               \
    template <>                                                                                                        \
    struct SC::Reflection::Reflect<StructName> : SC::Reflection::ReflectStruct<StructName>                             \
    {                                                                                                                  \
        template <typename TypeVisitor>                                                                                \
        static constexpr bool visit(TypeVisitor&& builder)                                                             \
        {                                                                                                              \
            SC_COMPILER_WARNING_PUSH_OFFSETOF                                                                          \
            return true

/// @brief Describes a single `MEMBER` of structure, giving it an `ORDER` ordinal. Can exist after SC_META_STRUCT_VISIT
#define SC_META_STRUCT_FIELD(ORDER, MEMBER) and builder(ORDER, #MEMBER, &T::MEMBER, SC_COMPILER_OFFSETOF(T, MEMBER))

/// @brief Closes description created by SC_META_STRUCT_VISIT
#define SC_META_STRUCT_LEAVE()                                                                                         \
    ;                                                                                                                  \
    SC_COMPILER_WARNING_POP                                                                                            \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
