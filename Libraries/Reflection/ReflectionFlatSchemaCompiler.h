// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/ConstexprTypes.h"
#include "../Foundation/Span.h" // TODO: Remove Span.h dependency
#include "Reflection.h"

namespace SC
{
namespace Reflection
{
template <typename Atom, typename Payload, int MAX_TOTAL_ATOMS>
struct FlatSchemaCompilerResult
{
    ConstexprArray<Atom, MAX_TOTAL_ATOMS> atoms;
    Payload                               payload;
};

template <typename MetaClassBuilder>
struct FlatSchemaCompiler
{
    typedef typename MetaClassBuilder::Atom     Atom;
    typedef typename Atom::MetaClassBuildFunc   MetaClassBuildFunc;
    typedef decltype(MetaClassBuilder::payload) PayloadType;
    template <int MAX_TOTAL_ATOMS>
    struct FlatSchema
    {
        ConstexprArray<MetaProperties, MAX_TOTAL_ATOMS>      properties;
        ConstexprArray<ConstexprStringView, MAX_TOTAL_ATOMS> names;
        PayloadType                                          payload;

        constexpr auto propertiesAsSpan() const
        {
            return Span<const MetaProperties>(properties.values, properties.size);
        }
        constexpr auto namesAsSpan() const { return Span<const ConstexprStringView>(names.values, names.size); }
    };

    template <int MAX_ATOMS>
    [[nodiscard]] static constexpr bool appendAtomsTo(ConstexprArray<Atom, MAX_ATOMS>& atoms, MetaClassBuildFunc build,
                                                      MetaClassBuilder& container)
    {
        container.initialSize = atoms.size;
        container.atoms.init(atoms.values + container.initialSize, MAX_ATOMS - container.initialSize);
        build(container);
        if (container.atoms.capacityWasEnough())
        {
            atoms.values[container.initialSize].properties.numSubAtoms = container.atoms.size - 1;
            atoms.size += container.atoms.size;
            return true;
        }
        return false;
    }

    [[nodiscard]] static constexpr int countAtoms(const Atom& atom)
    {
        if (atom.build != nullptr)
        {
            MetaClassBuilder builder;
            atom.build(builder);
            return builder.atoms.wantedCapacity;
        }
        return 0;
    }

    template <int MAX_LINK_BUFFER_SIZE, int MAX_TOTAL_ATOMS, typename Func>
    constexpr static FlatSchemaCompilerResult<Atom, PayloadType, MAX_TOTAL_ATOMS> compileAllAtomsFor(Func f)
    {
        FlatSchemaCompilerResult<Atom, PayloadType, MAX_TOTAL_ATOMS> result;

        MetaClassBuilder container(result.atoms.values, MAX_TOTAL_ATOMS);

        ConstexprArray<MetaClassBuildFunc, MAX_LINK_BUFFER_SIZE> alreadyVisitedTypes;
        ConstexprArray<int, MAX_LINK_BUFFER_SIZE>                alreadyVisitedLinkID;
        if (not appendAtomsTo(result.atoms, f, container))
        {
            return {};
        }
        int atomIndex = 1;
        while (atomIndex < result.atoms.size)
        {
            Atom&      atom        = result.atoms.values[atomIndex];
            const bool isEmptyLink = atom.properties.getLinkIndex() < 0;
            int        numSubAtoms = 0;
            if (isEmptyLink && (numSubAtoms = countAtoms(atom)) > 0)
            {
                int outIndex = -1;
                if (alreadyVisitedTypes.contains(atom.build, &outIndex))
                {
                    atom.properties.setLinkIndex(alreadyVisitedLinkID.values[outIndex]);
                }
                else
                {
                    atom.properties.setLinkIndex(result.atoms.size);
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
        result.payload = container.payload;
        return result;
    }

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, int MAX_LINK_BUFFER_SIZE = 20, int MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr auto schema =
            compileAllAtomsFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>(&MetaClass<T>::template build<MetaClassBuilder>);
        static_assert(schema.atoms.size > 0, "Something failed in compileAllAtomsFor");
        FlatSchema<schema.atoms.size> result;
        for (int i = 0; i < schema.atoms.size; ++i)
        {
            result.properties.values[i] = schema.atoms.values[i].properties;
            result.names.values[i]      = schema.atoms.values[i].name;
        }
        result.properties.size = schema.atoms.size;
        result.names.size      = schema.atoms.size;
        result.payload         = schema.payload;
        return result;
    }
};

} // namespace Reflection

} // namespace SC
