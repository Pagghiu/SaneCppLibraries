#pragma once
#include "Reflection.h"

namespace SC
{

namespace Reflection
{

template <int TOTAL_ATOMS>
struct FlatSchema
{
    MetaArray<MetaProperties, TOTAL_ATOMS> properties;
    MetaArray<MetaStringView, TOTAL_ATOMS> names;
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
            for (int idx = 0; idx < atomsChildren.values[0].properties.numSubAtoms; ++idx)
            {
                const auto& atom = atomsChildren.values[idx + 1];

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
                    if (atom.properties.type == MetaType::TypeInvalid)
                    {
                        return -1; // Missing metaclass for type
                    }
                    else if (linkAtoms.size > 0)
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

    // You can customize MAX_POSSIBLE_LINKS for the max numer of unique types in the system
    // This is just consuming compile time memory, so it doesn't matter putting any value as long as your compiler
    // is able to handle it without running out of heap space :)
    template <typename T, int MAX_POSSIBLE_LINKS = 500>
    static constexpr auto compile()
    {
        constexpr auto linkAtoms = MetaClassGetAtoms<T, MAX_ATOMS>();
        static_assert(linkAtoms.size > 0, "Missing metaclass for root class");
        constexpr auto UNIQUE_LINKS_NUMBER = countUniqueLinks<MAX_POSSIBLE_LINKS>(linkAtoms);
        static_assert(UNIQUE_LINKS_NUMBER >= 0, "Missing metaclass for a class reachable by root class");
        constexpr auto links = findAllLinks<UNIQUE_LINKS_NUMBER, MAX_POSSIBLE_LINKS>(linkAtoms, &MetaClass<T>::build);
        constexpr auto TOTAL_ATOMS =
            links.values[links.size - 1].flatternedIndex + links.values[links.size - 1].numberOfAtoms;
        FlatSchema<TOTAL_ATOMS> result;
        mergeLinksFlat(links, result.properties, &result.names);
        return result;
    }
};
} // namespace Reflection
} // namespace SC
