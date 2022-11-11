#pragma once
#include "Array.h"
#include "ReflectionSC.h"
#include "Result.h"
#include "SerializationTemplateCompiler.h"
#include "Vector.h"

namespace SC
{
namespace SerializationTemplate
{
template <typename BinaryStream, typename T, typename SFINAESelector = void>
struct Serializer;

template <typename T, typename SFINAESelector = void>
struct ClassInfo;

// clang-format off
template <typename T> struct IsPrimitive : false_type {};

template <> struct IsPrimitive<uint8_t>  : true_type  {};
template <> struct IsPrimitive<uint16_t> : true_type  {};
template <> struct IsPrimitive<uint32_t> : true_type  {};
template <> struct IsPrimitive<uint64_t> : true_type  {};
template <> struct IsPrimitive<int8_t>   : true_type  {};
template <> struct IsPrimitive<int16_t>  : true_type  {};
template <> struct IsPrimitive<int32_t>  : true_type  {};
template <> struct IsPrimitive<int64_t>  : true_type  {};
template <> struct IsPrimitive<float>    : true_type  {};
template <> struct IsPrimitive<double>   : true_type  {};
template <> struct IsPrimitive<char_t>   : true_type  {};
// clang-format on

template <typename T>
struct ClassInfoStruct
{
    size_t memberSizeSum = 0;
    bool   IsPacked      = false;

    constexpr ClassInfoStruct()
    {
        if (Reflection::MetaClass<T>::visit(*this))
        {
            IsPacked = memberSizeSum == sizeof(T);
        }
    }

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset)
    {
        if (not ClassInfo<R>().IsPacked)
        {
            return false;
        }
        memberSizeSum += sizeof(R);
        return true;
    }
};

template <typename T, typename SFINAESelector>
struct ClassInfo
{
    static constexpr bool IsPacked = ClassInfoStruct<T>().IsPacked;
};

template <typename T, int N>
struct ClassInfo<T[N]>
{
    static constexpr bool IsPacked = ClassInfo<T>::IsPacked;
};

template <typename T>
struct ClassInfo<T, typename SC::EnableIf<IsPrimitive<T>::value>::type>
{
    static constexpr bool IsPacked = true;
};

template <typename BinaryStream, typename T>
struct SerializerMemberIterator
{
    BinaryStream& stream;
    T&            object;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset) const
    {
        return Serializer<BinaryStream, R>::serialize(object.*member, stream);
    }
};

template <typename BinaryStream>
struct SimpleBinaryReaderSkipper
{
    Span<const Reflection::MetaProperties> sourceProperties;
    Reflection::MetaProperties             sourceProperty;

    SimpleBinaryReaderSkipper(BinaryStream& stream, int& sourceTypeIndex)
        : sourceObject(stream), sourceTypeIndex(sourceTypeIndex)
    {}

    [[nodiscard]] bool skip()
    {
        sourceProperty = sourceProperties.data[sourceTypeIndex];
        if (sourceProperty.type == Reflection::MetaType::TypeStruct)
        {
            return skipStruct();
        }
        else if (sourceProperty.type == Reflection::MetaType::TypeArray ||
                 sourceProperty.type == Reflection::MetaType::TypeVector)
        {
            return skipVectorOrArray();
        }
        else if (sourceProperty.isPrimitiveType())
        {
            return sourceObject.advance(sourceProperty.size);
        }
        return false;
    }

  private:
    BinaryStream& sourceObject;
    int&          sourceTypeIndex;

    [[nodiscard]] bool skipStruct()
    {
        const auto structSourceProperty  = sourceProperty;
        const auto structSourceTypeIndex = sourceTypeIndex;

        for (int16_t idx = 0; idx < structSourceProperty.numSubAtoms; ++idx)
        {
            sourceTypeIndex = structSourceTypeIndex + idx + 1;
            if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
            SC_TRY_IF(skip());
        }
        return true;
    }

    [[nodiscard]] bool skipVectorOrArray()
    {
        const auto arraySourceProperty  = sourceProperty;
        const auto arraySourceTypeIndex = sourceTypeIndex;

        sourceTypeIndex         = arraySourceTypeIndex + 1;
        uint64_t sourceNumBytes = arraySourceProperty.size;
        if (arraySourceProperty.type == Reflection::MetaType::TypeVector)
        {
            SC_TRY_IF(sourceObject.readAndAdvance(sourceNumBytes));
        }

        const bool isPrimitive = sourceProperties.data[sourceTypeIndex].isPrimitiveType();

        if (isPrimitive)
        {
            return sourceObject.advance(sourceNumBytes);
        }
        else
        {
            const auto sourceItemSize      = sourceProperties.data[sourceTypeIndex].size;
            const auto sourceNumElements   = sourceNumBytes / sourceItemSize;
            const auto itemSourceTypeIndex = sourceTypeIndex;
            for (uint64_t idx = 0; idx < sourceNumElements; ++idx)
            {
                sourceTypeIndex = itemSourceTypeIndex;
                if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
                SC_TRY_IF(skip());
            }
            return true;
        }
    }
};

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

    int sourceTypeIndex = 0;

    Reflection::MetaProperties current() const { return sourceProperties.data[sourceTypeIndex]; }

    void advance() { sourceTypeIndex++; }

    void resolveLink()
    {
        if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
            sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
    }

    template <typename BinaryStream>
    [[nodiscard]] bool skipCurrent(BinaryStream& stream)
    {
        SimpleBinaryReaderSkipper<BinaryStream> skipper(stream, sourceTypeIndex);
        skipper.sourceProperties = sourceProperties;
        return skipper.skip();
    }
};

template <typename BinaryStream, typename T>
struct SerializerVersionedMemberIterator
{
    VersionSchema& schema;
    BinaryStream&  stream;
    T&             object;
    int            matchOrder          = 0;
    bool           consumed            = false;
    bool           consumedWithSuccess = false;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset)
    {
        // TODO: If we code order inside visit, this can be made faster
        if (matchOrder == order)
        {
            consumed            = true;
            consumedWithSuccess = Serializer<BinaryStream, R>::serializeVersioned(object.*member, stream, schema);
            return false; // stop iterations
        }
        return true;
    }
};

template <typename BinaryStream, typename T, typename SFINAESelector>
struct Serializer
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serializeVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
    {
        typedef SerializerVersionedMemberIterator<BinaryStream, T> VersionedMemberIterator;
        if (schema.current().type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        const int numMembers      = schema.current().numSubAtoms;
        const int structTypeIndex = schema.sourceTypeIndex;
        for (int i = 0; i < numMembers; ++i)
        {
            schema.sourceTypeIndex          = structTypeIndex + i + 1;
            VersionedMemberIterator visitor = {schema, stream, object, schema.current().order};
            schema.resolveLink();
            Reflection::MetaClass<T>::visit(visitor);
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

    [[nodiscard]] static constexpr bool serialize(T& object, BinaryStream& stream)
    {
        if (IsItemPacked)
        {
            return stream.serialize({&object, sizeof(T)});
        }
        return Reflection::MetaClass<T>::visit(SerializerMemberIterator<BinaryStream, T>{stream, object});
    }
};

template <typename BinaryStream, typename T>
struct SerializerItems
{
    [[nodiscard]] static constexpr bool serializeItems(T* object, BinaryStream& stream, VersionSchema& schema,
                                                       uint32_t numSourceItems, uint32_t numDestinationItems)
    {
        schema.resolveLink();
        const auto commonSubsetItems  = min(numSourceItems, numDestinationItems);
        const auto arrayItemTypeIndex = schema.sourceTypeIndex;

        const bool isMemcpyable =
            IsPrimitive<T>::value && schema.current().type == Reflection::MetaClass<T>::getMetaType();
        if (isMemcpyable)
        {
            const auto sourceNumBytes = schema.current().size * numSourceItems;
            const auto destNumBytes   = numDestinationItems * sizeof(T);
            const auto minBytes       = min(static_cast<uint32_t>(destNumBytes), sourceNumBytes);
            SC_TRY_IF(stream.serialize({object, minBytes}));
            if (sourceNumBytes > static_cast<uint32_t>(destNumBytes))
            {
                SC_TRY_IF(schema.options.allowDropEccessArrayItems);
                return stream.advance(sourceNumBytes - minBytes);
            }
        }

        for (uint32_t idx = 0; idx < commonSubsetItems; ++idx)
        {
            schema.sourceTypeIndex = arrayItemTypeIndex;
            if (not Serializer<BinaryStream, T>::serializeVersioned(object[idx], stream, schema))
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
struct Serializer<BinaryStream, T[N]>
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serializeVersioned(T (&object)[N], BinaryStream& stream, VersionSchema& schema)
    {
        schema.advance(); // make T current type
        return SerializerItems<BinaryStream, T>::serializeVersioned(
            object, stream, schema, schema.current().getCustomUint32(), static_cast<uint32_t>(N));
    }

    [[nodiscard]] static constexpr bool serialize(T (&object)[N], BinaryStream& stream)
    {
        if (IsItemPacked)
        {
            return stream.serialize({object, sizeof(object)});
        }
        else
        {
            for (auto& item : object)
            {
                if (not Serializer<BinaryStream, T>::serialize(item, stream))
                    return false;
            }
            return true;
        }
    }
};

template <typename BinaryStream, typename Container, typename T>
struct SerializerVector
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serialize(Container& object, BinaryStream& stream)
    {
        const size_t itemSize    = sizeof(T);
        uint64_t     sizeInBytes = static_cast<uint64_t>(object.size() * itemSize);
        if (not Serializer<BinaryStream, uint64_t>::serialize(sizeInBytes, stream))
            return false;
        SC_TRY_IF(object.resize(sizeInBytes / itemSize));

        if (IsItemPacked)
        {
            return stream.serialize({object.data(), itemSize * object.size()});
        }
        else
        {
            for (auto& item : object)
            {
                if (!Serializer<BinaryStream, T>::serialize(item, stream))
                    return false;
            }
            return true;
        }
    }
};

template <typename BinaryStream, typename T>
struct Serializer<BinaryStream, SC::Vector<T>> : public SerializerVector<BinaryStream, SC::Vector<T>, T>
{
    [[nodiscard]] static constexpr bool serializeVersioned(SC::Vector<T>& object, BinaryStream& stream,
                                                           VersionSchema& schema)
    {
        uint64_t sizeInBytes = 0;
        if (not Serializer<BinaryStream, uint64_t>::serialize(sizeInBytes, stream))
            return false;
        schema.advance();
        const bool isMemcpyable =
            IsPrimitive<T>::value && schema.current().type == Reflection::MetaClass<T>::getMetaType();
        const size_t   sourceItemSize = schema.current().size;
        const uint32_t numSourceItems = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        if (isMemcpyable)
        {
            SC_TRY_IF(object.resizeWithoutInitializing(numSourceItems));
        }
        else
        {
            SC_TRY_IF(object.resize(numSourceItems));
        }
        return SerializerItems<BinaryStream, T>::serializeVersioned(object.data(), stream, schema, numSourceItems,
                                                                    numSourceItems);
    }
};

template <typename T>
struct ClassInfo<SC::Vector<T>>
{
    static constexpr bool IsPacked = false;
};

template <typename BinaryStream, typename T, int N>
struct Serializer<BinaryStream, SC::Array<T, N>> : public SerializerVector<BinaryStream, SC::Array<T, N>, T>
{
    [[nodiscard]] static constexpr bool serializeVersioned(SC::Array<T, N>& object, BinaryStream& stream,
                                                           VersionSchema& schema)
    {
        uint64_t sizeInBytes = 0;
        if (not Serializer<BinaryStream, uint64_t>::serialize(sizeInBytes, stream))
            return false;
        schema.advance();
        const bool isMemcpyable =
            IsPrimitive<T>::value && schema.current().type == Reflection::MetaClass<T>::getMetaType();

        const size_t   sourceItemSize      = schema.current().size;
        const uint32_t numSourceItems      = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        const uint32_t numDestinationItems = static_cast<uint32_t>(N);
        if (isMemcpyable)
        {
            SC_TRY_IF(object.resizeWithoutInitializing(min(numSourceItems, numDestinationItems)));
        }
        else
        {
            SC_TRY_IF(object.resize(min(numSourceItems, numDestinationItems)));
        }
        return SerializerItems<BinaryStream, T>::serializeItems(object.data(), stream, schema, numSourceItems,
                                                                numDestinationItems);
    }
};

template <typename T, int N>
struct ClassInfo<SC::Array<T, N>>
{
    static constexpr bool IsPacked = false;
};

template <typename BinaryStream, typename T>
struct Serializer<BinaryStream, T, typename SC::EnableIf<IsPrimitive<T>::value>::type>
{
    template <typename ValueType>
    [[nodiscard]] static bool readCastValue(T& destination, BinaryStream& stream)
    {
        ValueType value;
        SC_TRY_IF(stream.template readAndAdvance<ValueType>(value));
        destination = static_cast<T>(value);
        return true;
    }

    [[nodiscard]] static constexpr bool serializeVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
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
        return stream.serialize({&object, sizeof(T)});
    }

    [[nodiscard]] static constexpr bool serialize(T& object, BinaryStream& stream)
    {
        return stream.serialize({&object, sizeof(T)});
    }
};
} // namespace SerializationTemplate
} // namespace SC
