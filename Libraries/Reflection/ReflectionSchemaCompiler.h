// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Reflection.h"

namespace SC
{
namespace Reflection
{
//! @addtogroup group_reflection
//! @{

/// @brief Creates a schema linking a series of SchemaType
/// @tparam SchemaBuilder The builder used to obtain Virtual Tables
template <typename SchemaBuilder>
struct SchemaCompiler
{
  private:
    using Type              = typename SchemaBuilder::Type;
    using TypeBuildFunction = typename Type::TypeBuildFunction;
    using VirtualTablesType = decltype(SchemaBuilder::vtables);

    /// @brief Holds results as a maxed out array of size `MAX_TOTAL_TYPES`
    template <uint32_t MAX_TOTAL_TYPES>
    struct FlatFullResult
    {
        ArrayWithSize<Type, MAX_TOTAL_TYPES> types;
        VirtualTablesType                    vtables;
    };

    /// @brief Holds only the actual `NUM_TYPES` `<=` `MAX_TOTAL_TYPES` reducing executable size.
    template <uint32_t NUM_TYPES>
    struct FlatTrimmedResult
    {
        ArrayWithSize<TypeInfo, NUM_TYPES>       typeInfos;
        ArrayWithSize<TypeStringView, NUM_TYPES> typeNames;
        VirtualTablesType                        vtables;
    };
    template <typename Iterator, typename BinaryPredicate>
    static constexpr void bubbleSort(Iterator first, Iterator last, BinaryPredicate predicate)
    {
        if (first >= last)
        {
            return;
        }
        bool doSwap = true;
        while (doSwap)
        {
            doSwap      = false;
            Iterator p0 = first;
            Iterator p1 = first + 1;
            while (p1 != last)
            {
                if (predicate(*p1, *p0))
                {
                    swap(*p1, *p0);
                    doSwap = true;
                }
                ++p0;
                ++p1;
            }
        }
    }
    template <uint32_t MAX_TYPES>
    [[nodiscard]] static constexpr bool appendTypesTo(ArrayWithSize<Type, MAX_TYPES>& types, TypeBuildFunction build,
                                                      SchemaBuilder& builder)
    {
        // Let builder write to a slice of our available space in types array
        const auto baseLinkID = types.size;
        builder.currentLinkID = types.size;
        builder.types         = {types.values + types.size, MAX_TYPES - types.size};

        if (build(builder))
        {
            // Set number of children for parent type and update types array size
            const auto numberOfTypes    = builder.currentLinkID - baseLinkID;
            const auto numberOfChildren = numberOfTypes - 1;
            if (numberOfChildren > static_cast<decltype(TypeInfo::numberOfChildren)>(~0ull))
                return false;
            if (not types.values[baseLinkID].typeInfo.setNumberOfChildren(numberOfChildren))
                return false;

            struct OrderByMemberOffset
            {
                constexpr bool operator()(const Type& a, const Type& b) const
                {
                    return a.typeInfo.memberInfo.offsetInBytes < b.typeInfo.memberInfo.offsetInBytes;
                }
            };
            if (types.values[baseLinkID].typeInfo.type == TypeCategory::TypeStruct and
                types.values[baseLinkID].typeInfo.structInfo.isPacked)
            {
                // This is a little help for Binary Serialization, as packed structs end up serialized as is
                bubbleSort(types.values + baseLinkID + 1, types.values + baseLinkID + 1 + numberOfChildren,
                           OrderByMemberOffset());
            }
            types.size += numberOfTypes;
            return true;
        }
        return false;
    }

    template <uint32_t MAX_LINK_BUFFER_SIZE, uint32_t MAX_TOTAL_TYPES, typename Func>
    constexpr static FlatFullResult<MAX_TOTAL_TYPES> compileAllTypesFor(Func func)
    {
        // Collect all types
        FlatFullResult<MAX_TOTAL_TYPES> result;

        SchemaBuilder container(result.types.values, MAX_TOTAL_TYPES);

#if SC_COMPILER_GCC // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77911
        ArrayWithSize<TypeStringView, MAX_LINK_BUFFER_SIZE> alreadyVisitedTypes;
#else
        ArrayWithSize<TypeBuildFunction, MAX_LINK_BUFFER_SIZE> alreadyVisitedTypes;
#endif
        ArrayWithSize<uint32_t, MAX_LINK_BUFFER_SIZE> alreadyVisitedLinkID;
        if (not appendTypesTo(result.types, func, container))
        {
            return {};
        }

        // Link all collected types
        uint32_t typeIndex = 1;
        while (typeIndex < result.types.size)
        {
            Type& type = result.types.values[typeIndex];
            if (not type.typeInfo.isPrimitiveType() and type.typeInfo.needsLinking())
            {
                uint32_t outIndex = 0;
#if SC_COMPILER_GCC // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77911
                if (alreadyVisitedTypes.contains(type.typeName, &outIndex))
#else
                if (alreadyVisitedTypes.contains(type.typeBuild, &outIndex))
#endif
                {
                    if (not type.typeInfo.setLinkIndex(alreadyVisitedLinkID.values[outIndex]))
                        return {};
                }
                else
                {
                    if (not type.typeInfo.setLinkIndex(result.types.size))
                        return {};
                    if (not alreadyVisitedLinkID.push_back(result.types.size))
                        return {};
#if SC_COMPILER_GCC // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77911
                    if (not alreadyVisitedTypes.push_back(type.typeName))
#else
                    if (not alreadyVisitedTypes.push_back(type.typeBuild))
#endif
                        return {};
                    if (not appendTypesTo(result.types, type.typeBuild, container))
                        return {};
                }
            }
            typeIndex++;
        }
        result.vtables = container.vtables;
        return result;
    }

  public:
    /// @brief Returns a `constexpr` compiled trimmed flat schema for type `T`.
    /// @tparam T The type to be compiled
    /// @tparam MAX_LINK_BUFFER_SIZE Maximum number of "complex types" (anything that is not a primitive) that can be
    /// built
    /// @tparam MAX_TOTAL_TYPES Maximum number of types (struct members). When using constexpr it will trim it to actual
    /// size.
    /// @returns FlatTrimmedResult with only the required compiled types.
    template <typename T, uint32_t MAX_LINK_BUFFER_SIZE = 20, uint32_t MAX_TOTAL_TYPES = 100>
    static constexpr auto compile()
    {
        constexpr auto schema =
            compileAllTypesFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_TYPES>(&Reflect<T>::template build<SchemaBuilder>);
        static_assert(schema.types.size > 0, "Something failed in compileAllTypesFor");

        // Trim the returned FlatTrimmedResult only to the effective number of types
        FlatTrimmedResult<schema.types.size> result;
        for (uint32_t i = 0; i < schema.types.size; ++i)
        {
            result.typeInfos.values[i] = schema.types.values[i].typeInfo;
            result.typeNames.values[i] = schema.types.values[i].typeName;
        }
        result.typeInfos.size = schema.types.size;
        result.typeNames.size = schema.types.size;
        result.vtables        = schema.vtables;
        return result;
    }
};

/// @brief Holds together a TypeInfo, a StringView and a type-erased builder function pointer
/// @tparam TypeVisitor The type of member visitor that is parameter of the builder function
template <typename TypeVisitor>
struct SchemaType
{
    using TypeBuildFunction = bool (*)(TypeVisitor& builder);

    TypeInfo          typeInfo;
    TypeStringView    typeName;
    TypeBuildFunction typeBuild;

    constexpr SchemaType() : typeBuild(nullptr) {}
    constexpr SchemaType(const TypeInfo typeInfo, TypeStringView typeName, TypeBuildFunction typeBuild)
        : typeInfo(typeInfo), typeName(typeName), typeBuild(typeBuild)
    {}

    /// @brief Create from a generic type T
    template <typename T>
    [[nodiscard]] static constexpr SchemaType createGeneric()
    {
        return {TypeInfo(Reflect<T>::getCategory(), sizeof(T)), TypeToString<T>::get(), &Reflect<T>::build};
    }

    /// @brief Create from a Struct type T
    template <typename T>
    [[nodiscard]] static constexpr SchemaType createStruct(TypeStringView name = TypeToString<T>::get())
    {
        TypeInfo::StructInfo structInfo(ExtendedTypeInfo<T>::IsPacked);
        return {TypeInfo(Reflect<T>::getCategory(), sizeof(T), structInfo), name, &Reflect<T>::build};
    }

    /// @brief Create from a struct member with given name, memberTag and offset
    template <typename R, typename T, int N>
    [[nodiscard]] static constexpr SchemaType createMember(uint8_t memberTag, R T::*, const char (&name)[N],
                                                           size_t offset)
    {
        const auto info = TypeInfo::MemberInfo(memberTag, static_cast<SC::uint16_t>(offset));
        return {TypeInfo(Reflect<R>::getCategory(), sizeof(R), info), TypeStringView(name, N - 1), &Reflect<R>::build};
    }

    /// @brief Create from an array-like type
    template <typename T>
    [[nodiscard]] static constexpr SchemaType createArray(TypeStringView name, uint8_t numChildren,
                                                          TypeInfo::ArrayInfo arrayInfo)
    {
        return {TypeInfo(Reflect<T>::getCategory(), sizeof(T), numChildren, arrayInfo), name, &Reflect<T>::build};
    }
};

/// @brief Common code for derived class to create a SchemaBuilder suitable for SC::Reflection::SchemaCompiler
template <typename TypeVisitor>
struct SchemaBuilder
{
    using Type = SchemaType<TypeVisitor>;

    uint32_t currentLinkID;

    WritableRange<Type> types;

    constexpr SchemaBuilder(Type* output, const uint32_t capacity) : currentLinkID(0), types(output, capacity) {}

    template <typename R, typename T, int N>
    [[nodiscard]] constexpr bool operator()(uint8_t memberTag, R T::* field, const char (&name)[N], size_t offset)
    {
        currentLinkID++;
        return types.writeAndAdvance(Type::createMember(memberTag, field, name, offset));
    }

    [[nodiscard]] constexpr bool addType(Type type)
    {
        currentLinkID++;
        return types.writeAndAdvance(type);
    }
};

/// @brief A schema builder that doesn't build any virtual table
struct FlatSchemaBuilder : public SchemaBuilder<FlatSchemaBuilder>
{
    struct EmptyVTables
    {
    };
    EmptyVTables vtables;
    constexpr FlatSchemaBuilder(Type* output, const uint32_t capacity) : SchemaBuilder(output, capacity) {}
};

/// @brief Default schema not building any virtual table
using Schema = Reflection::SchemaCompiler<FlatSchemaBuilder>;
//! @}

} // namespace Reflection

} // namespace SC
