#pragma once
#include "Reflection.h"
namespace SC
{
namespace Reflection
{

template <int MAX_VTABLES>
struct ReflectionVTables
{
    ConstexprArray<VectorVTable, MAX_VTABLES> vector;
};

struct FlatSchemaTypeErased
{
    static const int MAX_VTABLES = 100;
    enum class MetaStructFlags : uint32_t
    {
        IsPacked            = 1 << 1, // No padding between members of a Struct
        IsRecursivelyPacked = 1 << 2, // IsPacked AND No padding in every contained field (recursively)
    };
    struct MetaClassBuilder1 : public MetaClassBuilder
    {
        ReflectionVTables<MAX_VTABLES> payload;
        constexpr MetaClassBuilder1(Atom* output = nullptr, const int capacity = 0) : MetaClassBuilder(output, capacity)
        {
            if (capacity > 0)
            {
                vectorVtable.init(payload.vector.values, MAX_VTABLES);
            }
        }
    };
    typedef FlatSchemaCompiler::FlatSchemaCompiler<MetaProperties, Atom, MetaClassBuilder1> FlatSchemaBase;

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, int MAX_LINK_BUFFER_SIZE = 20, int MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr auto schema =
            FlatSchemaBase::compileAllAtomsFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>(&MetaClass<T>::build);
        static_assert(schema.atoms.size > 0, "Something failed in compileAllAtomsFor");
        FlatSchemaBase::FlatSchema<schema.atoms.size> result;
        for (int i = 0; i < schema.atoms.size; ++i)
        {
            result.properties.values[i] = schema.atoms.values[i].properties;
            result.names.values[i]      = schema.atoms.values[i].name;
        }
        result.properties.size = schema.atoms.size;
        result.names.size      = schema.atoms.size;
        result.payload         = schema.payload;
        // TODO: This is really ugly
        while (schema.payload.vector.values[result.payload.vector.size++].resize != nullptr)
            ;
        markPackedStructs(result, 0);
        return result;
    }
    [[nodiscard]] static constexpr bool areAllMembersPacked(const MetaProperties* properties, int numAtoms)
    {
        uint32_t totalSize = 0;
        for (int idx = 0; idx < numAtoms; ++idx)
        {
            totalSize += properties[idx + 1].size;
        }
        return totalSize == properties->size;
    }

  private:
    template <int MAX_TOTAL_ATOMS>
    static constexpr bool markPackedStructs(FlatSchemaBase::FlatSchema<MAX_TOTAL_ATOMS>& result, int startIdx)
    {
        MetaProperties& atom = result.properties.values[startIdx];
        if (atom.isPrimitiveType())
        {
            return true; // packed by definition
        }
        else if (atom.type == MetaType::TypeStruct)
        {
            // packed if is itself packed and all of its non primitive members are packed
            if (not(atom.getCustomUint32() & static_cast<uint32_t>(MetaStructFlags::IsPacked)))
            {
                if (areAllMembersPacked(&atom, atom.numSubAtoms))
                {
                    atom.setCustomUint32(atom.getCustomUint32() | static_cast<uint32_t>(MetaStructFlags::IsPacked));
                }
            }
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
};

} // namespace Reflection
} // namespace SC
