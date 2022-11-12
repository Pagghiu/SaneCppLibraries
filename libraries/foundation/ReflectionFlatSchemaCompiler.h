#pragma once
#include "ConstexprTypes.h"
#include "Reflection.h"
#include "Span.h" // TODO: Remove Span.h dependency

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
};

} // namespace Reflection

} // namespace SC
