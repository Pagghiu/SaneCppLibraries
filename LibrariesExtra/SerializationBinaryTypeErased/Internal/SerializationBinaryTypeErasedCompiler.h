// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../../Libraries/Reflection/Reflection.h"
#include "../../../Libraries/Reflection/ReflectionSchemaCompiler.h"
namespace SC
{
namespace detail
{
struct SerializationBinaryTypeErasedVectorVTable
{
    enum class DropExcessItems
    {
        No,
        Yes
    };

    using FunctionGetSegmentSpan = bool (*)(Reflection::TypeInfo property, Span<char> object, Span<char>& itemBegin);
    using FunctionGetSegmentSpanConst = bool (*)(Reflection::TypeInfo property, Span<const char> object,
                                                 Span<const char>& itemBegin);

    using FunctionResize = bool (*)(Span<char> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                                    DropExcessItems dropExcessItems);
    using FunctionResizeWithoutInitialize = bool (*)(Span<char> object, Reflection::TypeInfo property,
                                                     uint64_t sizeInBytes, DropExcessItems dropExcessItems);
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

    [[nodiscard]] bool getSegmentSpan(uint32_t linkID, Reflection::TypeInfo property, Span<char> object,
                                      Span<char>& itemBegin);
    [[nodiscard]] bool getSegmentSpan(uint32_t linkID, Reflection::TypeInfo property, Span<const char> object,
                                      Span<const char>& itemBegin);

    using DropExcessItems = VectorVTable::DropExcessItems;
    enum class Initialize
    {
        No,
        Yes
    };

    bool resize(uint32_t linkID, Span<char> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                Initialize initialize, DropExcessItems dropExcessItems);
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
        vector.getSegmentSpan      = &getSegmentSpan<char>;
        vector.getSegmentSpanConst = &getSegmentSpan<const char>;
        vector.linkID              = builder.currentLinkID;
        return builder.vtables.vector.push_back(vector);
    }

    static bool resize(Span<char> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                       VectorVTable::DropExcessItems dropExcessItems)
    {
        SC_COMPILER_UNUSED(property);
        SC_COMPILER_UNUSED(dropExcessItems);
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

    static bool resizeWithoutInitialize(Span<char> object, Reflection::TypeInfo property, uint64_t sizeInBytes,
                                        VectorVTable::DropExcessItems dropExcessItems)
    {
        SC_COMPILER_UNUSED(property);
        SC_COMPILER_UNUSED(dropExcessItems);
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
