// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
// This needs to go before the compiler
#include "../../Reflection/ReflectionSC.h" // TODO: Split the SC Containers specifics in separate header
// Compiler must be after
#include "../../Reflection/ReflectionSchemaCompiler.h"
#include "SerializationBinarySchema.h"

#include "../../Foundation/Result.h"
namespace SC
{

namespace detail
{
template <typename BinaryStream, typename T, typename SFINAESelector = void>
struct SerializerBinaryReadVersioned;

template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerBinaryReadVersioned
{
    [[nodiscard]] static constexpr bool readVersioned(T& object, BinaryStream& stream, SerializationSchema& schema)
    {
        if (schema.current().type != Reflection::TypeCategory::TypeStruct)
            return false;
        const uint32_t numMembers      = static_cast<uint32_t>(schema.current().getNumberOfChildren());
        const auto     structTypeIndex = schema.sourceTypeIndex;
        for (uint32_t idx = 0; idx < numMembers; ++idx)
        {
            schema.sourceTypeIndex = structTypeIndex + idx + 1;
            MemberIterator visitor = {schema, stream, object, schema.current().memberInfo.memberTag};
            schema.resolveLink();
            Reflection::Reflect<T>::visit(visitor);
            if (visitor.consumed)
            {
                if (not visitor.consumedWithSuccess)
                    return false;
            }
            else
            {
                if (not schema.options.allowDropExcessStructMembers)
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
        SerializationSchema& schema;

        BinaryStream& stream;
        T&            object;

        int  matchMemberTag      = 0;
        bool consumed            = false;
        bool consumedWithSuccess = false;

        template <typename R, int N>
        constexpr bool operator()(int memberTag, R T::*field, const char (&)[N], size_t offset)
        {
            SC_COMPILER_UNUSED(offset);
            if (matchMemberTag == memberTag)
            {
                consumed = true;
                consumedWithSuccess =
                    SerializerBinaryReadVersioned<BinaryStream, R>::readVersioned(object.*field, stream, schema);
                return false; // stop iterations
            }
            return true;
        }
    };
};

template <typename BinaryStream, typename T>
struct SerializerReadVersionedItems
{
    [[nodiscard]] static constexpr bool readVersioned(T* object, BinaryStream& stream, SerializationSchema& schema,
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
                if (not schema.options.allowDropExcessArrayItems)
                    return false;
                return stream.advanceBytes(sourceNumBytes - minBytes);
            }
            return true;
        }

        for (uint32_t idx = 0; idx < commonSubsetItems; ++idx)
        {
            schema.sourceTypeIndex = arrayItemTypeIndex;
            if (not SerializerBinaryReadVersioned<BinaryStream, T>::readVersioned(object[idx], stream, schema))
                return false;
        }

        if (numSourceItems > numDestinationItems)
        {
            // We must consume these excess items anyway, discarding their content
            if (not schema.options.allowDropExcessArrayItems)
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
struct SerializerBinaryReadVersioned<BinaryStream, T[N]>
{
    [[nodiscard]] static constexpr bool readVersioned(T (&object)[N], BinaryStream& stream, SerializationSchema& schema)
    {
        schema.advance(); // make T current type
        return SerializerReadVersionedItems<BinaryStream, T>::readVersioned(
            object, stream, schema, schema.current().arrayInfo.numElements, static_cast<uint32_t>(N));
    }
};

template <typename BinaryStream, typename T>
struct SerializerBinaryReadVersioned<BinaryStream, SC::Vector<T>>
{
    [[nodiscard]] static constexpr bool readVersioned(SC::Vector<T>& object, BinaryStream& stream,
                                                      SerializationSchema& schema)
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
struct SerializerBinaryReadVersioned<BinaryStream, SC::Array<T, N>>
{
    [[nodiscard]] static constexpr bool readVersioned(SC::Array<T, N>& object, BinaryStream& stream,
                                                      SerializationSchema& schema)
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
struct SerializerBinaryReadVersioned<BinaryStream, T,
                                     typename SC::TypeTraits::EnableIf<Reflection::IsPrimitive<T>::value>::type>
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

    [[nodiscard]] static constexpr bool readVersioned(T& object, BinaryStream& stream, SerializationSchema& schema)
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
                if(schema.options.allowFloatToIntTruncation or TypeTraits::IsSame<T, float>::value or TypeTraits::IsSame<T, double>::value)
                {
                    return readCastValue<float>(object, stream);
                }
                return false;
            }
            case Reflection::TypeCategory::TypeDOUBLE64:
            {
                if(schema.options.allowFloatToIntTruncation or TypeTraits::IsSame<T, float>::value or TypeTraits::IsSame<T, double>::value)
                {
                    return readCastValue<double>(object, stream);
                }
                return false;
            }
            case Reflection::TypeCategory::TypeBOOL:
            {
                if(schema.options.allowBoolConversions or TypeTraits::IsSame<T, bool>::value)
                {
                    return readCastValue<bool>(object, stream);
                }
                return false;
            }
            case Reflection::TypeCategory::TypeInvalid:
            case Reflection::TypeCategory::TypeStruct:
            case Reflection::TypeCategory::TypeArray:
            case Reflection::TypeCategory::TypeVector:
            break;
        }
        // clang-format on
        SC_ASSERT_DEBUG(false);
        return false;
    }
};
} // namespace detail

} // namespace SC
