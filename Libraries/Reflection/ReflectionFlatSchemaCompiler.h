// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Reflection.h"

namespace SC
{
namespace Reflection
{
template <typename Atom, typename VTablesType, uint32_t MAX_TOTAL_ATOMS>
struct FlatSchemaCompilerResult
{
    SizedArray<Atom, MAX_TOTAL_ATOMS> atoms;
    VTablesType                       vtables;
};

template <typename MetaClassBuilder>
struct FlatSchemaCompiler
{
    using Atom               = typename MetaClassBuilder::Atom;
    using MetaClassBuildFunc = typename Atom::MetaClassBuildFunc;
    using PayloadType        = decltype(MetaClassBuilder::vtables);
    template <uint32_t MAX_TOTAL_ATOMS>
    struct FlatSchema
    {
        SizedArray<MetaProperties, MAX_TOTAL_ATOMS>   properties;
        SizedArray<SymbolStringView, MAX_TOTAL_ATOMS> names;
        PayloadType                                   vtables;
    };

    template <uint32_t MAX_ATOMS>
    [[nodiscard]] static constexpr bool appendAtomsTo(SizedArray<Atom, MAX_ATOMS>& atoms, MetaClassBuildFunc build,
                                                      MetaClassBuilder& container)
    {
        container.initialSize = atoms.size;
        container.atoms.init(atoms.values + container.initialSize, MAX_ATOMS - container.initialSize);
        build(container);
        if (container.atoms.capacityWasEnough())
        {
            const auto     lastAtomIndex = container.atoms.size - 1;
            constexpr auto maxSubAtoms   = static_cast<decltype(MetaProperties::numSubAtoms)>(MaxValue());
            if (lastAtomIndex > maxSubAtoms)
                return false;
            atoms.values[container.initialSize].properties.numSubAtoms =
                static_cast<decltype(MetaProperties::numSubAtoms)>(lastAtomIndex);
            atoms.size += container.atoms.size;
            return true;
        }
        return false;
    }

    [[nodiscard]] static constexpr uint32_t countAtoms(const Atom& atom)
    {
        if (atom.build != nullptr)
        {
            MetaClassBuilder builder;
            atom.build(builder);
            return builder.atoms.wantedCapacity;
        }
        return 0;
    }

    template <uint32_t MAX_LINK_BUFFER_SIZE, uint32_t MAX_TOTAL_ATOMS, typename Func>
    constexpr static FlatSchemaCompilerResult<Atom, PayloadType, MAX_TOTAL_ATOMS> compileAllAtomsFor(Func f)
    {
        FlatSchemaCompilerResult<Atom, PayloadType, MAX_TOTAL_ATOMS> result;

        MetaClassBuilder container(result.atoms.values, MAX_TOTAL_ATOMS);

        SizedArray<MetaClassBuildFunc, MAX_LINK_BUFFER_SIZE> alreadyVisitedTypes;
        SizedArray<uint32_t, MAX_LINK_BUFFER_SIZE>           alreadyVisitedLinkID;
        if (not appendAtomsTo(result.atoms, f, container))
        {
            return {};
        }
        uint32_t atomIndex = 1;
        while (atomIndex < result.atoms.size)
        {
            Atom&      atom        = result.atoms.values[atomIndex];
            const bool isEmptyLink = atom.properties.getLinkIndex() < 0;
            if (isEmptyLink and countAtoms(atom) > 0)
            {
                uint32_t outIndex = 0;
                if (alreadyVisitedTypes.contains(atom.build, &outIndex))
                {
                    if (not atom.properties.setLinkIndex(alreadyVisitedLinkID.values[outIndex]))
                        return {};
                }
                else
                {
                    if (not atom.properties.setLinkIndex(result.atoms.size))
                        return {};
                    if (not alreadyVisitedLinkID.push_back(result.atoms.size))
                        return {};
                    if (not alreadyVisitedTypes.push_back(atom.build))
                        return {};
                    if (not appendAtomsTo(result.atoms, atom.build, container))
                        return {};
                }
            }
            atomIndex++;
        }
        result.vtables = container.vtables;
        return result;
    }

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, uint32_t MAX_LINK_BUFFER_SIZE = 20, uint32_t MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr auto schema =
            compileAllAtomsFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>(&MetaClass<T>::template build<MetaClassBuilder>);
        static_assert(schema.atoms.size > 0, "Something failed in compileAllAtomsFor");
        FlatSchema<schema.atoms.size> result;
        for (uint32_t i = 0; i < schema.atoms.size; ++i)
        {
            result.properties.values[i] = schema.atoms.values[i].properties;
            result.names.values[i]      = schema.atoms.values[i].name;
        }
        result.properties.size = schema.atoms.size;
        result.names.size      = schema.atoms.size;
        result.vtables         = schema.vtables;
        return result;
    }
};
struct FlatSchemaClassBuilder : public MetaClassBuilder<FlatSchemaClassBuilder>
{
    using Atom = AtomBase<FlatSchemaClassBuilder>;
    constexpr FlatSchemaClassBuilder(Atom* output = nullptr, const uint32_t capacity = 0)
        : MetaClassBuilder(output, capacity)
    {}
};

using FlatSchema = Reflection::FlatSchemaCompiler<FlatSchemaClassBuilder>;

} // namespace Reflection

} // namespace SC
