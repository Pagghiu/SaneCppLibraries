#pragma once
#include "ConstexprTypes.h"
#include "Span.h" // TODO: Remove Span.h dependency

namespace SC
{
namespace FlatSchemaCompilerBase
{
template <typename MetaProperties, typename Atom, typename MetaClassBuilder>
struct FlatSchemaCompilerBase
{
    template <int MAX_TOTAL_ATOMS>
    struct FlatSchema
    {
        ConstexprArray<MetaProperties, MAX_TOTAL_ATOMS>      properties;
        ConstexprArray<ConstexprStringView, MAX_TOTAL_ATOMS> names;

        constexpr auto propertiesAsSpan() const
        {
            return Span<const MetaProperties>(properties.values, properties.size);
        }
        constexpr auto namesAsSpan() const { return Span<const ConstexprStringView>(names.values, names.size); }
    };

    template <int MAX_ATOMS>
    [[nodiscard]] static constexpr bool appendAtomsTo(ConstexprArray<Atom, MAX_ATOMS>&  atoms,
                                                      typename Atom::MetaClassBuildFunc build)
    {
        const int        initialSize = atoms.size;
        MetaClassBuilder container(atoms.values + initialSize, MAX_ATOMS - initialSize);
        build(container);
        if (container.wantedCapacity == container.size)
        {
            atoms.values[initialSize].properties.numSubAtoms = container.size - 1;
            atoms.size += container.size;
            return true;
        }
        return false;
    }

    template <int MAX_LINK_BUFFER_SIZE, int MAX_TOTAL_ATOMS, typename Func>
    constexpr static ConstexprArray<Atom, MAX_TOTAL_ATOMS> compileAllAtomsFor(Func f)
    {
        ConstexprArray<Atom, MAX_TOTAL_ATOMS>                                   allAtoms;
        ConstexprArray<typename Atom::MetaClassBuildFunc, MAX_LINK_BUFFER_SIZE> alreadyVisitedTypes;
        ConstexprArray<int, MAX_LINK_BUFFER_SIZE>                               alreadyVisitedLinkID;
        if (not appendAtomsTo(allAtoms, f))
        {
            return {};
        }
        int atomIndex = 1;
        while (atomIndex < allAtoms.size)
        {
            Atom&      atom        = allAtoms.values[atomIndex];
            const bool isEmptyLink = atom.properties.getLinkIndex() < 0;
            int        numSubAtoms = 0;
            if (isEmptyLink && (numSubAtoms = MetaClassBuilder::countAtoms(atom)) > 0)
            {
                int outIndex = -1;
                if (alreadyVisitedTypes.contains(atom.build, &outIndex))
                {
                    atom.properties.setLinkIndex(alreadyVisitedLinkID.values[outIndex]);
                }
                else
                {
                    atom.properties.setLinkIndex(allAtoms.size);
                    if (not alreadyVisitedLinkID.push_back(allAtoms.size))
                        return {};
                    if (not alreadyVisitedTypes.push_back(atom.build))
                        return {};
                    if (not appendAtomsTo(allAtoms, atom.build))
                        return {};
                }
            }
            atomIndex++;
        }
        return allAtoms;
    }
};

} // namespace FlatSchemaCompilerBase

} // namespace SC
