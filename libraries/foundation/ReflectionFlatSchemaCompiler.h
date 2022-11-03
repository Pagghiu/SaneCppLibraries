#pragma once
#include "Reflection.h"
#include "Span.h" // TODO: Remove Span.h dependency

namespace SC
{

namespace Reflection
{

template <int TOTAL_ATOMS>
struct FlatSchema
{
    MetaArray<MetaProperties, TOTAL_ATOMS> properties;
    MetaArray<MetaStringView, TOTAL_ATOMS> names;

    constexpr auto propertiesAsSpan() const { return Span<const MetaProperties>(properties.values, properties.size); }
    constexpr auto namesAsSpan() const { return Span<const MetaStringView>(names.values, names.size); }
};

struct FlatAtomLink
{
    typedef typename Atom::MetaClassBuildFunc MetaClassBuildFunc;

    MetaClassBuildFunc build;
    int                flatternedIndex;
    int                numberOfAtoms;

    constexpr FlatAtomLink() : build(nullptr), flatternedIndex(0), numberOfAtoms(0) {}

    template <int MAX_ATOMS>
    constexpr auto getAtoms() const
    {
        return MetaBuild<MAX_ATOMS>(build);
    }
};

// You can customize MAX_ATOMS to match the max number of atoms (+1) of any single MetaClass that will be linked
template <int MAX_ATOMS = 20>
struct FlatSchemaCompiler
{
    template <int MAX_POSSIBLE_LINKS>
    static constexpr int countUniqueLinks(const MetaArray<Atom, MAX_ATOMS>& rootAtom)
    {
        MetaArray<MetaArray<Atom, MAX_ATOMS>, MAX_POSSIBLE_LINKS> atomsQueue;
        atomsQueue.values[atomsQueue.size++] = rootAtom;
        MetaArray<typename Atom::MetaClassBuildFunc, MAX_POSSIBLE_LINKS> alreadyVisitedLinks;

        int numLinks = 1;

        while (atomsQueue.size > 0)
        {
            atomsQueue.size--;
            const auto atomsChildren = atomsQueue.values[atomsQueue.size]; // MUST copy as we're modifying queue
            if (atomsChildren.values[0].properties.type == MetaType::TypeInvalid)
            {
                return -1;
            }
            for (int idx = 0; idx < atomsChildren.values[0].properties.numSubAtoms; ++idx)
            {
                const auto& atom = atomsChildren.values[idx + 1];
                if (atom.properties.type == MetaType::TypeInvalid)
                {
                    return -1; // Missing metaclass for type
                }
                bool alreadyVisitedLink = false;
                for (int searchIDX = 0; searchIDX < alreadyVisitedLinks.size; ++searchIDX)
                {
                    if (alreadyVisitedLinks.values[searchIDX] == atom.build)
                    {
                        alreadyVisitedLink = true;
                        break;
                    }
                }
                if (not alreadyVisitedLink)
                {
                    alreadyVisitedLinks.values[alreadyVisitedLinks.size++] = atom.build;
                    const auto linkAtoms                                   = atom.template getAtoms<MAX_ATOMS>();
                    if (linkAtoms.size > 0)
                    {
                        numLinks++;
                        atomsQueue.values[atomsQueue.size++] = linkAtoms;
                    }
                    else if (atom.properties.type == MetaType::TypeStruct)
                    {
                        return -2; // Created a struct with empty list of atoms
                    }
                }
            }
        }
        return numLinks;
    }

    template <int UNIQUE_LINKS_NUMBER, int MAX_POSSIBLE_LINKS>
    static constexpr auto findAllLinks(const MetaArray<Atom, MAX_ATOMS>& inputAtoms,
                                       typename Atom::MetaClassBuildFunc rootBuilder)
    {
        MetaArray<FlatAtomLink, UNIQUE_LINKS_NUMBER>              links;
        MetaArray<MetaArray<Atom, MAX_ATOMS>, MAX_POSSIBLE_LINKS> atomsQueue;
        atomsQueue.values[atomsQueue.size++]     = inputAtoms;
        links.values[links.size].build           = rootBuilder;
        links.values[links.size].flatternedIndex = 0;
        links.values[links.size].numberOfAtoms   = inputAtoms.size;
        links.size++;
        while (atomsQueue.size > 0)
        {
            atomsQueue.size--;
            const auto rootAtom = atomsQueue.values[atomsQueue.size]; // MUST be copy (we modify atomsQueue)
            for (int atomIdx = 0; atomIdx < rootAtom.values[0].properties.numSubAtoms; ++atomIdx)
            {
                const auto& atom      = rootAtom.values[atomIdx + 1];
                auto        build     = atom.build;
                bool        foundLink = false;
                for (int searchIDX = 0; searchIDX < links.size; ++searchIDX)
                {
                    if (links.values[searchIDX].build == build)
                    {
                        foundLink = true;
                        break;
                    }
                }
                if (not foundLink)
                {
                    auto linkAtoms = atom.template getAtoms<MAX_ATOMS>();
                    if (linkAtoms.size > 0)
                    {
                        atomsQueue.values[atomsQueue.size++] = linkAtoms;
                        int prevMembers                      = 0;
                        if (links.size > 0)
                        {
                            const auto& prev = links.values[links.size - 1];
                            prevMembers      = prev.flatternedIndex + prev.numberOfAtoms;
                        }

                        links.values[links.size].build           = build;
                        links.values[links.size].flatternedIndex = prevMembers;
                        links.values[links.size].numberOfAtoms   = linkAtoms.size;
                        links.size++;
                    }
                }
            }
        }
        return links;
    }

    template <int TOTAL_ATOMS, int MAX_LINKS_NUMBER>
    static constexpr void mergeLinksFlat(const MetaArray<FlatAtomLink, MAX_LINKS_NUMBER>& links,
                                         MetaArray<MetaProperties, TOTAL_ATOMS>&          mergedProps,
                                         MetaArray<MetaStringView, TOTAL_ATOMS>*          mergedNames)
    {
        for (int linkIndex = 0; linkIndex < links.size; ++linkIndex)
        {
            auto linkAtoms                         = links.values[linkIndex].template getAtoms<MAX_ATOMS>();
            mergedProps.values[mergedProps.size++] = linkAtoms.values[0].properties;
            if (mergedNames)
            {
                mergedNames->values[mergedNames->size++] = linkAtoms.values[0].name;
            }
            for (int atomIndex = 0; atomIndex < linkAtoms.values[0].properties.numSubAtoms; ++atomIndex)
            {
                const auto& field                    = linkAtoms.values[1 + atomIndex];
                mergedProps.values[mergedProps.size] = field.properties;
                if (mergedNames)
                {
                    mergedNames->values[mergedNames->size++] = field.name;
                }
                for (int findIdx = 0; findIdx < links.size; ++findIdx)
                {
                    if (links.values[findIdx].build == field.build)
                    {
                        mergedProps.values[mergedProps.size].setLinkIndex(links.values[findIdx].flatternedIndex);
                        break;
                    }
                }
                mergedProps.size++;
            }
        }
    }

    template <int TOTAL_ATOMS>
    static constexpr bool markPackedStructs(FlatSchema<TOTAL_ATOMS>& result, int startIdx)
    {
        MetaProperties& atom = result.properties.values[startIdx];
        if (IsPrimitiveType(atom.type))
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
                if (not IsPrimitiveType(member.type))
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

    // You can customize MAX_POSSIBLE_LINKS for the max numer of unique types in the system
    // This is just consuming compile time memory, so it doesn't matter putting any value as long as your compiler
    // is able to handle it without running out of heap space :)
    template <typename T, int MAX_POSSIBLE_LINKS = 500>
    static constexpr auto compile()
    {
        constexpr auto linkAtoms = MetaClassGetAtoms<T, MAX_ATOMS>();
        static_assert(linkAtoms.size > 0, "Missing metaclass for root class");
        constexpr auto MISSING_METACLASS = countUniqueLinks<MAX_POSSIBLE_LINKS>(linkAtoms);
        static_assert(MISSING_METACLASS >= 0, "Missing metaclass for a class reachable by root class");
        constexpr auto UNIQUE_LINKS_NUMBER = MISSING_METACLASS < 0 ? 1 : MISSING_METACLASS;
        constexpr auto links = findAllLinks<UNIQUE_LINKS_NUMBER, MAX_POSSIBLE_LINKS>(linkAtoms, &MetaClass<T>::build);
        constexpr auto TOTAL_ATOMS =
            links.values[links.size - 1].flatternedIndex + links.values[links.size - 1].numberOfAtoms;
        FlatSchema<TOTAL_ATOMS> result;
        mergeLinksFlat(links, result.properties, &result.names);
        markPackedStructs(result, 0);
        return result;
    }
};
} // namespace Reflection
} // namespace SC
