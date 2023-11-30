// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
// This needs to go before the compiler
#include "../Reflection/ReflectionSC.h" // TODO: Split the SC Containers specifics in separate header
// Compiler must be after
#include "../Reflection/ReflectionSchemaCompiler.h"
#include "SerializationBinarySkipper.h"

namespace SC
{
namespace SerializationBinary
{

template <typename BinaryStream, typename T, typename SFINAESelector = void>
struct SerializerReadVersioned;

/// @brief Holds Schema of serialized binary data
struct VersionSchema
{
    struct Options
    {
        bool allowFloatToIntTruncation    = true;
        bool allowDropEccessArrayItems    = true;
        bool allowDropEccessStructMembers = true;
    };
    Options options;

    Span<const Reflection::TypeInfo> sourceTypes;

    uint32_t sourceTypeIndex = 0;

    constexpr Reflection::TypeInfo current() const { return sourceTypes.data()[sourceTypeIndex]; }

    constexpr void advance() { sourceTypeIndex++; }

    constexpr void resolveLink()
    {
        if (sourceTypes.data()[sourceTypeIndex].hasValidLinkIndex())
            sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());
    }

    template <typename BinaryStream>
    [[nodiscard]] constexpr bool skipCurrent(BinaryStream& stream)
    {
        Serialization::BinarySkipper<BinaryStream> skipper(stream, sourceTypeIndex);
        skipper.sourceTypes = sourceTypes;
        return skipper.skip();
    }
};

/// @brief De-serializes binary data with its associated schema into object `T`
template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerReadVersioned
{
    [[nodiscard]] static constexpr bool readVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
    {
        if (schema.current().type != Reflection::TypeCategory::TypeStruct)
            return false;
        const uint32_t numMembers      = static_cast<uint32_t>(schema.current().getNumberOfChildren());
        const auto     structTypeIndex = schema.sourceTypeIndex;
        for (uint32_t idx = 0; idx < numMembers; ++idx)
        {
            schema.sourceTypeIndex = structTypeIndex + idx + 1;
            MemberIterator visitor = {schema, stream, object, schema.current().memberInfo.order};
            schema.resolveLink();
            Reflection::Reflect<T>::visit(visitor);
            if (visitor.consumed)
            {
                if (not visitor.consumedWithSuccess)
                    return false;
            }
            else
            {
                if (not schema.options.allowDropEccessStructMembers)
                    return false;
                // We must consume it anyway, discarding its content
                if (not schema.skipCurrent(stream))
                    return false;
            }
        }
        return true;
    }

  private:
    struct MemberIterator
    {
        VersionSchema& schema;
        BinaryStream&  stream;
        T&             object;

        int  matchOrder          = 0;
        bool consumed            = false;
        bool consumedWithSuccess = false;

        template <typename R, int N>
        constexpr bool operator()(int order, R T::*field, const char (&)[N], size_t offset)
        {
            SC_COMPILER_UNUSED(offset);
            if (matchOrder == order)
            {
                consumed = true;
                consumedWithSuccess =
                    SerializerReadVersioned<BinaryStream, R>::readVersioned(object.*field, stream, schema);
                return false; // stop iterations
            }
            return true;
        }
    };
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
            Reflection::IsPrimitive<T>::value && schema.current().type == Reflection::Reflect<T>::getCategory();
        if (isPacked)
        {
            const size_t sourceNumBytes = schema.current().sizeInBytes * numSourceItems;
            const size_t destNumBytes   = numDestinationItems * sizeof(T);
            const size_t minBytes       = min(destNumBytes, sourceNumBytes);
            if (not stream.serializeBytes(object, minBytes))
                return false;
            if (sourceNumBytes > destNumBytes)
            {
                // We must consume these excess bytes anyway, discarding their content
                if (not schema.options.allowDropEccessArrayItems)
                    return false;
                return stream.advanceBytes(sourceNumBytes - minBytes);
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
            if (not schema.options.allowDropEccessArrayItems)
                return false;

            for (uint32_t idx = 0; idx < numSourceItems - numDestinationItems; ++idx)
            {
                schema.sourceTypeIndex = arrayItemTypeIndex;
                if (not schema.skipCurrent(stream))
                    return false;
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
            object, stream, schema, schema.current().arrayInfo.numElements, static_cast<uint32_t>(N));
    }
};

template <typename BinaryStream, typename T>
struct SerializerReadVersioned<BinaryStream, SC::Vector<T>>
{
    [[nodiscard]] static constexpr bool readVersioned(SC::Vector<T>& object, BinaryStream& stream,
                                                      VersionSchema& schema)
    {
        uint64_t sizeInBytes = 0;
        SC_TRY(stream.serializeBytes(&sizeInBytes, sizeof(sizeInBytes)));
        schema.advance();
        const bool isPacked =
            Reflection::IsPrimitive<T>::value && schema.current().type == Reflection::Reflect<T>::getCategory();
        const size_t   sourceItemSize = schema.current().sizeInBytes;
        const uint32_t numSourceItems = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        if (isPacked)
        {
            SC_TRY(object.resizeWithoutInitializing(numSourceItems));
        }
        else
        {
            SC_TRY(object.resize(numSourceItems));
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
        if (not stream.serializeBytes(&sizeInBytes, sizeof(sizeInBytes)))
            return false;
        schema.advance();
        const bool isPacked =
            Reflection::IsPrimitive<T>::value && schema.current().type == Reflection::Reflect<T>::getCategory();

        const size_t   sourceItemSize      = schema.current().sizeInBytes;
        const uint32_t numSourceItems      = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        const uint32_t numDestinationItems = static_cast<uint32_t>(N);
        if (isPacked)
        {
            if (not object.resizeWithoutInitializing(min(numSourceItems, numDestinationItems)))
                return false;
        }
        else
        {
            if (not object.resize(min(numSourceItems, numDestinationItems)))
                return false;
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
        if (not stream.serializeBytes(&value, sizeof(ValueType)))
            return false;
        destination = static_cast<T>(value);
        return true;
    }

    [[nodiscard]] static constexpr bool readVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
    {
        // clang-format off
        switch (schema.current().type)
        {
            case Reflection::TypeCategory::TypeUINT8:      return readCastValue<uint8_t>(object, stream);
            case Reflection::TypeCategory::TypeUINT16:     return readCastValue<uint16_t>(object, stream);
            case Reflection::TypeCategory::TypeUINT32:     return readCastValue<uint32_t>(object, stream);
            case Reflection::TypeCategory::TypeUINT64:     return readCastValue<uint64_t>(object, stream);
            case Reflection::TypeCategory::TypeINT8:       return readCastValue<int8_t>(object, stream);
            case Reflection::TypeCategory::TypeINT16:      return readCastValue<int16_t>(object, stream);
            case Reflection::TypeCategory::TypeINT32:      return readCastValue<int32_t>(object, stream);
            case Reflection::TypeCategory::TypeINT64:      return readCastValue<int64_t>(object, stream);
            case Reflection::TypeCategory::TypeFLOAT32:
            {
                if(schema.options.allowFloatToIntTruncation || IsSame<T, float>::value || IsSame<T, double>::value)
                {
                    return readCastValue<float>(object, stream);
                }
                return false;
            }
            case Reflection::TypeCategory::TypeDOUBLE64:
            {
                if(schema.options.allowFloatToIntTruncation || IsSame<T, float>::value || IsSame<T, double>::value)
                {
                    return readCastValue<double>(object, stream);
                }
                return false;
            }
            default:
                SC_ASSERT_DEBUG(false);
            return false;
        }
        // clang-format on
    }
};

} // namespace SerializationBinary
} // namespace SC
