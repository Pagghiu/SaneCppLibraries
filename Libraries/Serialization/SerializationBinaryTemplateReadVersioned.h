// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
// This needs to go before the compiler
#include "../Reflection/ReflectionSC.h"
// Compiler must be after
#include "SerializationBinarySkipper.h"
#include "SerializationBinaryTemplateCompiler.h"

namespace SC
{
namespace SerializationBinaryTemplate
{

template <typename BinaryStream, typename T, typename SFINAESelector = void>
struct SerializerReadVersioned;

struct VersionSchema
{
    struct Options
    {
        bool allowFloatToIntTruncation    = true;
        bool allowDropEccessArrayItems    = true;
        bool allowDropEccessStructMembers = true;
    };
    Options options;

    Span<const Reflection::MetaProperties> sourceProperties;

    uint32_t sourceTypeIndex = 0;

    constexpr Reflection::MetaProperties current() const { return sourceProperties.data()[sourceTypeIndex]; }

    constexpr void advance() { sourceTypeIndex++; }

    constexpr void resolveLink()
    {
        if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
            sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
    }

    template <typename BinaryStream>
    [[nodiscard]] constexpr bool skipCurrent(BinaryStream& stream)
    {
        Serialization::BinarySkipper<BinaryStream> skipper(stream, sourceTypeIndex);
        skipper.sourceProperties = sourceProperties;
        return skipper.skip();
    }
};

template <typename BinaryStream, typename T>
struct SerializerReadVersionedMemberIterator
{
    VersionSchema& schema;
    BinaryStream&  stream;
    int            matchOrder          = 0;
    bool           consumed            = false;
    bool           consumedWithSuccess = false;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R& field)
    {
        SC_UNUSED(name);
        if (matchOrder == order)
        {
            consumed            = true;
            consumedWithSuccess = SerializerReadVersioned<BinaryStream, R>::readVersioned(field, stream, schema);
            return false; // stop iterations
        }
        return true;
    }
};

template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerReadVersioned
{
    [[nodiscard]] static constexpr bool readVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
    {
        typedef SerializerReadVersionedMemberIterator<BinaryStream, T> VersionedMemberIterator;
        if (schema.current().type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        const uint32_t numMembers      = static_cast<uint32_t>(schema.current().numSubAtoms);
        const auto     structTypeIndex = schema.sourceTypeIndex;
        for (uint32_t idx = 0; idx < numMembers; ++idx)
        {
            schema.sourceTypeIndex          = structTypeIndex + idx + 1;
            VersionedMemberIterator visitor = {schema, stream, schema.current().order};
            schema.resolveLink();
            Reflection::MetaClass<T>::visitObject(visitor, object);
            if (visitor.consumed)
            {
                SC_TRY_IF(visitor.consumedWithSuccess);
            }
            else
            {
                SC_TRY_IF(schema.options.allowDropEccessStructMembers)
                // We must consume it anyway, discarding its content
                SC_TRY_IF(schema.skipCurrent(stream));
            }
        }
        return true;
    }
};

template <typename BinaryStream, typename T>
struct SerializerReadVersionedItems
{
    [[nodiscard]] static constexpr bool readVersioned(T* object, BinaryStream& stream, VersionSchema& schema,
                                                      uint32_t numSourceItems, uint32_t numDestinationItems)
    {
        schema.resolveLink();
        const auto commonSubsetItems  = min(numSourceItems, numDestinationItems);
        const auto arrayItemTypeIndex = schema.sourceTypeIndex;

        const bool isPacked =
            Reflection::IsPrimitive<T>::value && schema.current().type == Reflection::MetaClass<T>::getMetaType();
        if (isPacked)
        {
            const size_t sourceNumBytes = schema.current().sizeInBytes * numSourceItems;
            const size_t destNumBytes   = numDestinationItems * sizeof(T);
            const size_t minBytes       = min(destNumBytes, sourceNumBytes);
            SC_TRY_IF(stream.serialize({object, minBytes}));
            if (sourceNumBytes > destNumBytes)
            {
                // We must consume these excess bytes anyway, discarding their content
                SC_TRY_IF(schema.options.allowDropEccessArrayItems);
                return stream.advance(sourceNumBytes - minBytes);
            }
            return true;
        }

        for (uint32_t idx = 0; idx < commonSubsetItems; ++idx)
        {
            schema.sourceTypeIndex = arrayItemTypeIndex;
            if (not SerializerReadVersioned<BinaryStream, T>::readVersioned(object[idx], stream, schema))
                return false;
        }

        if (numSourceItems > numDestinationItems)
        {
            // We must consume these excess items anyway, discarding their content
            SC_TRY_IF(schema.options.allowDropEccessArrayItems);

            for (uint32_t idx = 0; idx < numSourceItems - numDestinationItems; ++idx)
            {
                schema.sourceTypeIndex = arrayItemTypeIndex;
                SC_TRY_IF(schema.skipCurrent(stream));
            }
        }
        return true;
    }
};

template <typename BinaryStream, typename T, int N>
struct SerializerReadVersioned<BinaryStream, T[N]>
{
    [[nodiscard]] static constexpr bool readVersioned(T (&object)[N], BinaryStream& stream, VersionSchema& schema)
    {
        schema.advance(); // make T current type
        return SerializerReadVersionedItems<BinaryStream, T>::readVersioned(
            object, stream, schema, schema.current().getCustomUint32(), static_cast<uint32_t>(N));
    }
};

template <typename BinaryStream, typename T>
struct SerializerReadVersioned<BinaryStream, SC::Vector<T>>
{
    [[nodiscard]] static constexpr bool readVersioned(SC::Vector<T>& object, BinaryStream& stream,
                                                      VersionSchema& schema)
    {
        uint64_t sizeInBytes = 0;
        SC_TRY_IF(stream.serialize({&sizeInBytes, sizeof(sizeInBytes)}));
        schema.advance();
        const bool isPacked =
            Reflection::IsPrimitive<T>::value && schema.current().type == Reflection::MetaClass<T>::getMetaType();
        const size_t   sourceItemSize = schema.current().sizeInBytes;
        const uint32_t numSourceItems = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        if (isPacked)
        {
            SC_TRY_IF(object.resizeWithoutInitializing(numSourceItems));
        }
        else
        {
            SC_TRY_IF(object.resize(numSourceItems));
        }
        return SerializerReadVersionedItems<BinaryStream, T>::readVersioned(object.data(), stream, schema,
                                                                            numSourceItems, numSourceItems);
    }
};

template <typename BinaryStream, typename T, int N>
struct SerializerReadVersioned<BinaryStream, SC::Array<T, N>>
{
    [[nodiscard]] static constexpr bool readVersioned(SC::Array<T, N>& object, BinaryStream& stream,
                                                      VersionSchema& schema)
    {
        uint64_t sizeInBytes = 0;
        SC_TRY_IF(stream.serialize({&sizeInBytes, sizeof(sizeInBytes)}));
        schema.advance();
        const bool isPacked =
            Reflection::IsPrimitive<T>::value && schema.current().type == Reflection::MetaClass<T>::getMetaType();

        const size_t   sourceItemSize      = schema.current().sizeInBytes;
        const uint32_t numSourceItems      = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        const uint32_t numDestinationItems = static_cast<uint32_t>(N);
        if (isPacked)
        {
            SC_TRY_IF(object.resizeWithoutInitializing(min(numSourceItems, numDestinationItems)));
        }
        else
        {
            SC_TRY_IF(object.resize(min(numSourceItems, numDestinationItems)));
        }
        return SerializerReadVersionedItems<BinaryStream, T>::readVersioned(object.data(), stream, schema,
                                                                            numSourceItems, numDestinationItems);
    }
};

template <typename BinaryStream, typename T>
struct SerializerReadVersioned<BinaryStream, T, typename SC::EnableIf<Reflection::IsPrimitive<T>::value>::type>
{
    template <typename ValueType>
    [[nodiscard]] static bool readCastValue(T& destination, BinaryStream& stream)
    {
        ValueType value;
        SC_TRY_IF(stream.serialize({&value, sizeof(ValueType)}));
        destination = static_cast<T>(value);
        return true;
    }

    [[nodiscard]] static constexpr bool readVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
    {
        // clang-format off
        switch (schema.current().type)
        {
            case Reflection::MetaType::TypeUINT8:      return readCastValue<uint8_t>(object, stream);
            case Reflection::MetaType::TypeUINT16:     return readCastValue<uint16_t>(object, stream);
            case Reflection::MetaType::TypeUINT32:     return readCastValue<uint32_t>(object, stream);
            case Reflection::MetaType::TypeUINT64:     return readCastValue<uint64_t>(object, stream);
            case Reflection::MetaType::TypeINT8:       return readCastValue<int8_t>(object, stream);
            case Reflection::MetaType::TypeINT16:      return readCastValue<int16_t>(object, stream);
            case Reflection::MetaType::TypeINT32:      return readCastValue<int32_t>(object, stream);
            case Reflection::MetaType::TypeINT64:      return readCastValue<int64_t>(object, stream);
            case Reflection::MetaType::TypeFLOAT32:
            {
                if(schema.options.allowFloatToIntTruncation || IsSame<T, float>::value || IsSame<T, double>::value)
                {
                    return readCastValue<float>(object, stream);
                }
                return false;
            }
            case Reflection::MetaType::TypeDOUBLE64:
            {
                if(schema.options.allowFloatToIntTruncation || IsSame<T, float>::value || IsSame<T, double>::value)
                {
                    return readCastValue<double>(object, stream);
                }
                return false;
            }
            default: return false;
        }
        // clang-format on
    }
};

} // namespace SerializationBinaryTemplate
} // namespace SC
