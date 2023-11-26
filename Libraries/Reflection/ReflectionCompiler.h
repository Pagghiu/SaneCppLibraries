// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Reflection.h"

namespace SC
{
namespace Reflection
{

/// @brief Creates a schema linking a series of ReflectedType
/// @tparam SchemaBuilder The builder used to obtain Virtual Tables
template <typename SchemaBuilder>
struct Compiler
{
    using Type              = typename SchemaBuilder::Type;
    using TypeBuildFunction = typename Type::TypeBuildFunction;
    using VirtualTablesType = decltype(SchemaBuilder::vtables);

    template <uint32_t MAX_TOTAL_TYPES>
    struct FlatSchema
    {
        ArrayWithSize<TypeInfo, MAX_TOTAL_TYPES>       typeInfos;
        ArrayWithSize<TypeStringView, MAX_TOTAL_TYPES> typeNames;
        VirtualTablesType                              vtables;
    };

    template <uint32_t MAX_TOTAL_TYPES>
    struct Result
    {
        ArrayWithSize<Type, MAX_TOTAL_TYPES> types;
        VirtualTablesType                    vtables;
    };

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
            if (numberOfChildren > static_cast<decltype(TypeInfo::numberOfChildren)>(MaxValue()))
                return false;
            if (not types.values[baseLinkID].typeInfo.setNumberOfChildren(numberOfChildren))
                return false;
            types.size += numberOfTypes;
            return true;
        }
        return false;
    }

    template <uint32_t MAX_LINK_BUFFER_SIZE, uint32_t MAX_TOTAL_TYPES, typename Func>
    constexpr static Result<MAX_TOTAL_TYPES> compileAllTypesFor(Func func)
    {
        // Collect all types
        Result<MAX_TOTAL_TYPES> result;

        SchemaBuilder container(result.types.values, MAX_TOTAL_TYPES);

        ArrayWithSize<TypeBuildFunction, MAX_LINK_BUFFER_SIZE> alreadyVisitedTypes;
        ArrayWithSize<uint32_t, MAX_LINK_BUFFER_SIZE>          alreadyVisitedLinkID;
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
                if (alreadyVisitedTypes.contains(type.typeBuild, &outIndex))
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
                    if (not alreadyVisitedTypes.push_back(type.typeBuild))
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

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_TYPES: maximum number of types (struct members). When using constexpr it will trim it to actual size.
    template <typename T, uint32_t MAX_LINK_BUFFER_SIZE = 20, uint32_t MAX_TOTAL_TYPES = 100>
    static constexpr auto compile()
    {
        constexpr auto schema =
            compileAllTypesFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_TYPES>(&Reflect<T>::template build<SchemaBuilder>);
        static_assert(schema.types.size > 0, "Something failed in compileAllTypesFor");

        // Trim the returned FlatSchema only to the effective number of types
        FlatSchema<schema.types.size> result;
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

/// @brief Common code for derived class to create a SchemaBuilder suitable for SC::Reflection::Compiler
template <typename TypeVisitor>
struct SchemaBuilder
{
    using Type = ReflectedType<TypeVisitor>;

    uint32_t currentLinkID;

    WritableRange<Type> types;

    constexpr SchemaBuilder(Type* output, const uint32_t capacity) : currentLinkID(0), types(output, capacity) {}

    template <typename R, typename T, int N>
    [[nodiscard]] constexpr bool operator()(uint8_t order, const char (&name)[N], R T::*field, size_t offset)
    {
        currentLinkID++;
        return types.writeAndAdvance(Type::createMember(order, name, field, offset));
    }

    [[nodiscard]] constexpr bool addType(Type type)
    {
        currentLinkID++;
        return types.writeAndAdvance(type);
    }
};

/// @brief A schema builder that doesn't build any virtual table
struct DefaultSchemaBuilder : public SchemaBuilder<DefaultSchemaBuilder>
{
    struct EmptyVTables
    {
    };
    EmptyVTables vtables;
    constexpr DefaultSchemaBuilder(Type* output, const uint32_t capacity) : SchemaBuilder(output, capacity) {}
};

/// @brief Default schema not building any virtual table
using Schema = Reflection::Compiler<DefaultSchemaBuilder>;

} // namespace Reflection

} // namespace SC
