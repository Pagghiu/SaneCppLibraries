#pragma once
#include "Reflection.h"
namespace SC
{
namespace Reflection
{

struct VectorVTable
{
    enum class DropEccessItems
    {
        No,
        Yes
    };

    typedef bool (*FunctionGetSegmentSpan)(MetaProperties property, Span<void> object, Span<void>& itemBegin);
    typedef bool (*FunctionGetSegmentSpanConst)(MetaProperties property, Span<const void> object,
                                                Span<const void>& itemBegin);

    typedef bool (*FunctionResize)(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                                   DropEccessItems dropEccessItems);
    typedef bool (*FunctionResizeWithoutInitialize)(Span<void> object, Reflection::MetaProperties property,
                                                    uint64_t sizeInBytes, DropEccessItems dropEccessItems);
    FunctionGetSegmentSpan          getSegmentSpan;
    FunctionGetSegmentSpanConst     getSegmentSpanConst;
    FunctionResize                  resize;
    FunctionResizeWithoutInitialize resizeWithoutInitialize;
    uint32_t                        linkID;

    constexpr VectorVTable()
        : getSegmentSpan(nullptr), getSegmentSpanConst(nullptr), resize(nullptr), resizeWithoutInitialize(nullptr),
          linkID(0)
    {}
};
template <int MAX_VTABLES>
struct ReflectionVTables
{
    ConstexprArray<VectorVTable, MAX_VTABLES> vector;
};

struct MetaClassBuilderTypeErased : public MetaClassBuilder<MetaClassBuilderTypeErased>
{
    typedef AtomBase<MetaClassBuilderTypeErased> Atom;
    static const int                             MAX_VTABLES = 100;
    ReflectionVTables<MAX_VTABLES>               payload;
    MetaArrayView<VectorVTable>                  vectorVtable;

    constexpr MetaClassBuilderTypeErased(Atom* output = nullptr, const int capacity = 0)
        : MetaClassBuilder(output, capacity)
    {
        if (capacity > 0)
        {
            vectorVtable.init(payload.vector.values, MAX_VTABLES);
        }
    }
};

template <typename Container, typename ItemType, int N>
struct VectorArrayVTable<MetaClassBuilderTypeErased, Container, ItemType, N>
{
    constexpr static void build(MetaClassBuilderTypeErased& builder)
    {
        if (builder.vectorVtable.capacity > 0)
        {
            VectorVTable vector;
            vector.resize = &resize;
            assignResizeWithoutInitialize(vector);
            vector.getSegmentSpan      = &getSegmentSpan<void>;
            vector.getSegmentSpanConst = &getSegmentSpan<const void>;
            vector.linkID              = builder.initialSize + builder.atoms.size;
            builder.vectorVtable.push(vector);
        }
    }

    static bool resize(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                       VectorVTable::DropEccessItems dropEccessItems)
    {
        if (object.size >= sizeof(void*))
        {
            auto& vectorByte = *static_cast<Container*>(object.data);
            auto  numItems   = sizeInBytes / sizeof(ItemType);
            if (N >= 0)
                numItems = min(numItems, static_cast<decltype(numItems)>(N));
            return vectorByte.resize(numItems);
        }
        else
        {
            return false;
        }
    }
    static bool resizeWithoutInitialize(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                                        VectorVTable::DropEccessItems dropEccessItems)
    {
        if (object.size >= sizeof(void*))
        {
            auto& vectorByte = *static_cast<Container*>(object.data);
            auto  numItems   = sizeInBytes / sizeof(ItemType);
            if (N >= 0)
                numItems = min(numItems, static_cast<decltype(numItems)>(N));
            return vectorByte.resizeWithoutInitializing(numItems);
        }
        else
        {
            return false;
        }
    }

    template <typename VoidType>
    [[nodiscard]] static constexpr bool getSegmentSpan(Reflection::MetaProperties property, Span<VoidType> object,
                                                       Span<VoidType>& itemBegin)
    {
        if (object.size >= sizeof(void*))
        {
            typedef typename SameConstnessAs<VoidType, Container>::type VectorType;
            auto& vectorByte = *static_cast<VectorType*>(object.data);
            itemBegin        = Span<VoidType>(vectorByte.data(), vectorByte.size() * sizeof(ItemType));
            return true;
        }
        else
        {
            return false;
        }
    }
    template <typename Q = ItemType>
    [[nodiscard]] static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
        constexpr assignResizeWithoutInitialize(VectorVTable& vector)
    {}

    template <typename Q = ItemType>
    [[nodiscard]] static typename EnableIf<IsTriviallyCopyable<Q>::value, void>::type //
        constexpr assignResizeWithoutInitialize(VectorVTable& vector)
    {
        vector.resizeWithoutInitialize = &resizeWithoutInitialize;
    }
};

struct FlatSchemaTypeErased
{
    typedef Reflection::FlatSchemaCompiler<MetaProperties, MetaClassBuilderTypeErased::Atom, MetaClassBuilderTypeErased>
        FlatSchemaBase;

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, int MAX_LINK_BUFFER_SIZE = 20, int MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr auto schema = FlatSchemaBase::compileAllAtomsFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>(
            &MetaClass<T>::template build<MetaClassBuilderTypeErased>);
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
        return result;
    }
};

} // namespace Reflection
} // namespace SC
