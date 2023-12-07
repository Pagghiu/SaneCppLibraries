// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../../Libraries/Reflection/Reflection.h"
#include "../../../Libraries/Reflection/ReflectionSchemaCompiler.h"
namespace SC
{
namespace detail
{
struct SerializationBinaryTypeErasedVectorVTable
{
    enum class DropEccessItems
    {
        No,
        Yes
    };

    using FunctionGetSegmentSpan      = bool (*)(Reflection::TypeInfo property, Span<uint8_t> object,
                                            Span<uint8_t>& itemBegin);
    using FunctionGetSegmentSpanConst = bool (*)(Reflection::TypeInfo property, Span<const uint8_t> object,
                                                 Span<const uint8_t>& itemBegin);

    using FunctionResize = bool (*)(Span<uint8_t> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                                    DropEccessItems dropEccessItems);
    using FunctionResizeWithoutInitialize = bool (*)(Span<uint8_t> object, Reflection::TypeInfo property,
                                                     uint64_t sizeInBytes, DropEccessItems dropEccessItems);
    FunctionGetSegmentSpan          getSegmentSpan;
    FunctionGetSegmentSpanConst     getSegmentSpanConst;
    FunctionResize                  resize;
    FunctionResizeWithoutInitialize resizeWithoutInitialize;
    uint32_t                        linkID;

    constexpr SerializationBinaryTypeErasedVectorVTable()
        : getSegmentSpan(nullptr), getSegmentSpanConst(nullptr), resize(nullptr), resizeWithoutInitialize(nullptr),
          linkID(0)
    {}
};

template <int MAX_VTABLES>
struct SerializationBinaryTypeErasedReflectionVTables
{
    Reflection::ArrayWithSize<SerializationBinaryTypeErasedVectorVTable, MAX_VTABLES> vector;
};

struct SerializationBinaryTypeErasedArrayAccess
{
    using VectorVTable = SerializationBinaryTypeErasedVectorVTable;
    Span<const VectorVTable> vectorVtable;

    [[nodiscard]] bool getSegmentSpan(uint32_t linkID, Reflection::TypeInfo property, Span<uint8_t> object,
                                      Span<uint8_t>& itemBegin);
    [[nodiscard]] bool getSegmentSpan(uint32_t linkID, Reflection::TypeInfo property, Span<const uint8_t> object,
                                      Span<const uint8_t>& itemBegin);

    using DropEccessItems = VectorVTable::DropEccessItems;
    enum class Initialize
    {
        No,
        Yes
    };

    bool resize(uint32_t linkID, Span<uint8_t> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                Initialize initialize, DropEccessItems dropEccessItems);
};
} // namespace detail

namespace Reflection
{
struct FlatSchemaBuilderTypeErased : public SchemaBuilder<FlatSchemaBuilderTypeErased>
{
    using Type = SchemaType<FlatSchemaBuilderTypeErased>;

    static const uint32_t MAX_VTABLES = 100;

    detail::SerializationBinaryTypeErasedReflectionVTables<MAX_VTABLES> vtables;

    constexpr FlatSchemaBuilderTypeErased(Type* output, const uint32_t capacity) : SchemaBuilder(output, capacity) {}
};

template <typename Container, typename ItemType, int N>
struct VectorArrayVTable<FlatSchemaBuilderTypeErased, Container, ItemType, N>
{
    using VectorVTable = detail::SerializationBinaryTypeErasedVectorVTable;
    [[nodiscard]] constexpr static bool build(FlatSchemaBuilderTypeErased& builder)
    {
        VectorVTable vector;
        vector.resize = &resize;
        assignResizeWithoutInitialize(vector);
        vector.getSegmentSpan      = &getSegmentSpan<uint8_t>;
        vector.getSegmentSpanConst = &getSegmentSpan<const uint8_t>;
        vector.linkID              = builder.currentLinkID;
        return builder.vtables.vector.push_back(vector);
    }

    static bool resize(Span<uint8_t> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                       VectorVTable::DropEccessItems dropEccessItems)
    {
        SC_COMPILER_UNUSED(property);
        SC_COMPILER_UNUSED(dropEccessItems);
        if (object.sizeInBytes() >= sizeof(void*))
        {
            auto&      vectorByte = *reinterpret_cast<Container*>(object.data());
            const auto numItems   = N >= 0 ? min(sizeInBytes / sizeof(ItemType), static_cast<decltype(sizeInBytes)>(N))
                                           : sizeInBytes / sizeof(ItemType);
            return vectorByte.resize(static_cast<size_t>(numItems));
        }
        else
        {
            return false;
        }
    }

    static bool resizeWithoutInitialize(Span<uint8_t> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                                        VectorVTable::DropEccessItems dropEccessItems)
    {
        SC_COMPILER_UNUSED(property);
        SC_COMPILER_UNUSED(dropEccessItems);
        if (object.sizeInBytes() >= sizeof(void*))
        {
            auto&      vectorByte = *reinterpret_cast<Container*>(object.data());
            const auto numItems   = N >= 0 ? min(sizeInBytes / sizeof(ItemType), static_cast<decltype(sizeInBytes)>(N))
                                           : sizeInBytes / sizeof(ItemType);
            return vectorByte.resizeWithoutInitializing(static_cast<size_t>(numItems));
        }
        else
        {
            return false;
        }
    }

    template <typename ByteType>
    [[nodiscard]] static constexpr bool getSegmentSpan(Reflection::TypeInfo property, Span<ByteType> object,
                                                       Span<ByteType>& itemBegin)
    {
        SC_COMPILER_UNUSED(property);
        if (object.sizeInBytes() >= sizeof(void*))
        {
            using VectorType = typename TypeTraits::SameConstnessAs<ByteType, Container>::type;
            auto& vectorByte = *reinterpret_cast<VectorType*>(object.data());
            itemBegin        = itemBegin.reinterpret_bytes(vectorByte.data(), vectorByte.size() * sizeof(ItemType));
            return true;
        }
        else
        {
            return false;
        }
    }

    template <typename Q = ItemType>
    [[nodiscard]] static typename TypeTraits::EnableIf<not TypeTraits::IsTriviallyCopyable<Q>::value, void>::type //
        constexpr assignResizeWithoutInitialize(VectorVTable& vector)
    {
        SC_COMPILER_UNUSED(vector);
    }

    template <typename Q = ItemType>
    [[nodiscard]] static typename TypeTraits::EnableIf<TypeTraits::IsTriviallyCopyable<Q>::value, void>::type //
        constexpr assignResizeWithoutInitialize(VectorVTable& vector)
    {
        vector.resizeWithoutInitialize = &resizeWithoutInitialize;
    }
};

using SchemaTypeErased = Reflection::SchemaCompiler<FlatSchemaBuilderTypeErased>;

} // namespace Reflection

} // namespace SC
