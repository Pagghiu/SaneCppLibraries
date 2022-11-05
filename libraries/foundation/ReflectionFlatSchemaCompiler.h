#pragma once
#include "Reflection.h"
#include "Span.h" // TODO: Remove Span.h dependency

namespace SC
{
namespace Reflection
{

template <int MAX_TOTAL_ATOMS>
struct FlatSchema
{
    MetaArray<MetaProperties, MAX_TOTAL_ATOMS> properties;
    MetaArray<MetaStringView, MAX_TOTAL_ATOMS> names;

    constexpr auto propertiesAsSpan() const { return Span<const MetaProperties>(properties.values, properties.size); }
    constexpr auto namesAsSpan() const { return Span<const MetaStringView>(names.values, names.size); }
};

struct FlatSchemaCompiler
{
    // TODO: This is not necessary anymore with the templated Binary Serializer...
    template <int MAX_TOTAL_ATOMS>
    static constexpr bool markPackedStructs(FlatSchema<MAX_TOTAL_ATOMS>& result, int startIdx)
    {
        MetaProperties& atom = result.properties.values[startIdx];
        if (atom.isPrimitiveType())
        {
            return true; // packed by definition
        }
        else if (atom.type == MetaType::TypeStruct)
        {
            // packed if is itself packed and all of its non primitive members are packed
            const auto structFlags         = atom.getCustomUint32();
            bool       isRecursivelyPacked = true;
            if (not(structFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked)))
            {
                isRecursivelyPacked = false;
            }
            for (int idx = 0; idx < atom.numSubAtoms; ++idx)
            {
                const MetaProperties& member = result.properties.values[startIdx + 1 + idx];
                if (not member.isPrimitiveType())
                {
                    if (not markPackedStructs(result, member.getLinkIndex()))
                    {
                        isRecursivelyPacked = false;
                    }
                }
            }
            if (isRecursivelyPacked)
            {
                atom.setCustomUint32(structFlags | static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked));
            }
            return isRecursivelyPacked;
        }
        int             newIndex = startIdx + 1;
        MetaProperties& itemAtom = result.properties.values[startIdx + 1];
        if (itemAtom.getLinkIndex() > 0)
            newIndex = itemAtom.getLinkIndex();
        // We want to visit the inner type anyway
        const bool innerResult = markPackedStructs(result, newIndex);
        if (atom.type == MetaType::TypeArray)
        {
            return innerResult; // C-arrays are packed if their inner type is packed
        }
        else
        {
            return false; // Vector & co will break packed state
        }
    }

    template <typename T, int MAX_LINK_BUFFER_SIZE, int MAX_TOTAL_ATOMS>
    constexpr static MetaArray<Atom, MAX_TOTAL_ATOMS> compileAllAtomsFor()
    {
        MetaArray<Atom, MAX_TOTAL_ATOMS>                                   allAtoms;
        MetaArray<typename Atom::MetaClassBuildFunc, MAX_LINK_BUFFER_SIZE> alreadyVisitedTypes;
        MetaArray<int, MAX_LINK_BUFFER_SIZE>                               alreadyVisitedLinkID;
        if (not MetaBuildAppend(allAtoms, &MetaClass<T>::build))
        {
            return {};
        }
        int atomIndex = 1;
        while (atomIndex < allAtoms.size)
        {
            Atom&      atom        = allAtoms.values[atomIndex];
            const bool isEmptyLink = atom.properties.getLinkIndex() < 0;
            int        numSubAtoms = 0;
            if (isEmptyLink && (numSubAtoms = atom.countAtoms()) > 0)
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
                    if (not MetaBuildAppend(allAtoms, atom.build))
                        return {};
                }
            }
            atomIndex++;
        }
        return allAtoms;
    }

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, int MAX_LINK_BUFFER_SIZE = 20, int MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr MetaArray<Atom, MAX_TOTAL_ATOMS> allAtoms =
            compileAllAtomsFor<T, MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>();
        static_assert(allAtoms.size > 0, "Something failed in compileAllAtomsFor");
        FlatSchema<allAtoms.size> result;
        for (int i = 0; i < allAtoms.size; ++i)
        {
            result.properties.values[i] = allAtoms.values[i].properties;
            result.names.values[i]      = allAtoms.values[i].name;
        }
        result.properties.size = allAtoms.size;
        result.names.size      = allAtoms.size;
        // TODO: make markPackedStructs optional ? Or maybe drop it if we switch to templated serializer
        markPackedStructs(result, 0);
        return result;
    }
};

} // namespace Reflection
} // namespace SC
