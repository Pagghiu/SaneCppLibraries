#pragma once
#include "Array.h"
#include "Map.h"
#include "Reflection2.h"
#include "SerializationTestSuite.h"
#include "String.h"
#include "Test.h"
#include "Vector.h"

namespace SC
{
namespace Serialization2
{
template <typename BinaryStream, typename T, typename T2 = void>
struct Serializer;

template <typename BinaryStream, typename T>
struct SerializerMemberIterator;

template <typename T>
struct ClassInfoMembers;

template <typename T, typename T2 = void>
struct ClassInfo;

template <typename BinaryStream, typename Container, typename T>
struct SerializerVector;

struct VersionSchema;

template <typename T>
struct HashFor;
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

// TODO: Hardcode hashes for these types to improve compile time
template <> struct HashFor<uint8_t>  { static constexpr auto Hash = StringHash("uint8");  };
template <> struct HashFor<uint16_t> { static constexpr auto Hash = StringHash("uint16"); };
template <> struct HashFor<uint32_t> { static constexpr auto Hash = StringHash("uint32"); };
template <> struct HashFor<uint64_t> { static constexpr auto Hash = StringHash("uint64"); };
template <> struct HashFor<int8_t>   { static constexpr auto Hash = StringHash("int8");   };
template <> struct HashFor<int16_t>  { static constexpr auto Hash = StringHash("int16");  };
template <> struct HashFor<int32_t>  { static constexpr auto Hash = StringHash("int32");  };
template <> struct HashFor<int64_t>  { static constexpr auto Hash = StringHash("int64");  };
template <> struct HashFor<float>    { static constexpr auto Hash = StringHash("float");  };
template <> struct HashFor<double>   { static constexpr auto Hash = StringHash("double"); };

template <> struct HashFor<char_t>      : HashFor<uint8_t>{};
template <> struct IsPrimitive<char_t>  : IsPrimitive<uint8_t>  {};
// clang-format on

struct BinaryWriterStream
{
    size_t              index = 0;
    SC::Vector<uint8_t> buffer;
    int                 numberOfOperations = 0;

    [[nodiscard]] bool serialize(Span<const void> object)
    {
        numberOfOperations++;
        Span<const uint8_t> bytes = object.castTo<const uint8_t>();
        return buffer.appendCopy(bytes.data, bytes.size);
    }
};

struct BinaryReaderStream
{
    size_t              index = 0;
    SC::Vector<uint8_t> buffer;
    int                 numberOfOperations = 0;

    [[nodiscard]] bool serialize(Span<void> object)
    {
        if (index + object.size > buffer.size())
            return false;
        numberOfOperations++;
        Span<uint8_t> bytes = object.castTo<uint8_t>();
        memcpy(bytes.data, &buffer[index], bytes.size);
        index += bytes.size;
        return true;
    }

    [[nodiscard]] bool advance(size_t numBytes)
    {
        if (index + numBytes > buffer.size())
            return false;
        index += numBytes;
        return true;
    }

    template <typename T>
    [[nodiscard]] constexpr bool readAndAdvance(T& value)
    {
        return serialize(Span<void>{&value, sizeof(T)});
    }
};

template <typename T>
struct ClassInfoMembers
{
    size_t memberSizeSum = 0;
    bool   IsPacked      = false;

    constexpr ClassInfoMembers()
    {
        if (Reflection2::MetaClass<T>::visit(*this))
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

template <typename T, typename T2>
struct ClassInfo
{
    static constexpr bool IsPacked = ClassInfoMembers<T>().IsPacked;
    static constexpr auto Hash     = HashFor<T>::Hash;
};

template <typename T, int N>
struct ClassInfo<T[N]>
{
    static constexpr bool IsPacked = ClassInfo<T>::IsPacked;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("[]"), HashFor<T>::Hash, N);
};

template <typename T>
struct ClassInfo<T, typename SC::EnableIf<IsPrimitive<T>::value>::type>
{
    static constexpr bool IsPacked = true;
    static constexpr auto Hash     = HashFor<T>::Hash;
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
    Span<const Reflection2::MetaProperties> sourceProperties;
    Reflection2::MetaProperties             sourceProperty;

    BinaryStream& sourceObject;
    int&          sourceTypeIndex;

    SimpleBinaryReaderSkipper(BinaryStream& stream, int& sourceTypeIndex)
        : sourceObject(stream), sourceTypeIndex(sourceTypeIndex)
    {}

    [[nodiscard]] bool read()
    {
        sourceProperty = sourceProperties.data[sourceTypeIndex];
        switch (sourceProperty.type)
        {
        case Reflection2::MetaType::TypeInvalid: //
        {
            return false;
        }
        case Reflection2::MetaType::TypeUINT8:
        case Reflection2::MetaType::TypeUINT16:
        case Reflection2::MetaType::TypeUINT32:
        case Reflection2::MetaType::TypeUINT64:
        case Reflection2::MetaType::TypeINT8:
        case Reflection2::MetaType::TypeINT16:
        case Reflection2::MetaType::TypeINT32:
        case Reflection2::MetaType::TypeINT64:
        case Reflection2::MetaType::TypeFLOAT32:
        case Reflection2::MetaType::TypeDOUBLE64: //
        {
            SC_TRY_IF(sourceObject.advance(sourceProperty.size))
            break;
        }
        case Reflection2::MetaType::TypeStruct: //
        {
            return readStruct();
        }
        case Reflection2::MetaType::TypeArray:
        case Reflection2::MetaType::TypeVector: {
            return readArray();
        }
        }
        return true;
    }

    [[nodiscard]] bool readStruct()
    {
        const auto structSourceProperty  = sourceProperty;
        const auto structSourceTypeIndex = sourceTypeIndex;

        for (int16_t idx = 0; idx < structSourceProperty.numSubAtoms; ++idx)
        {
            sourceTypeIndex = structSourceTypeIndex + idx + 1;
            if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
            SC_TRY_IF(read());
        }
        return true;
    }

    [[nodiscard]] bool readArray()
    {
        const auto arraySourceProperty  = sourceProperty;
        const auto arraySourceTypeIndex = sourceTypeIndex;

        sourceTypeIndex         = arraySourceTypeIndex + 1;
        uint64_t sourceNumBytes = arraySourceProperty.size;
        if (arraySourceProperty.type == Reflection2::MetaType::TypeVector)
        {
            SC_TRY_IF(sourceObject.readAndAdvance(sourceNumBytes));
        }

        const bool isPrimitive = sourceProperties.data[sourceTypeIndex].isPrimitiveType();

        if (isPrimitive)
        {
            SC_TRY_IF(sourceObject.advance(sourceNumBytes));
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
                SC_TRY_IF(read());
            }
        }
        return true;
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

    Span<const Reflection2::MetaProperties> sourceProperties;

    int sourceTypeIndex = 0;

    Reflection2::MetaProperties current() const { return sourceProperties.data[sourceTypeIndex]; }

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
        return skipper.read();
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

template <typename BinaryStream, typename T, typename T2>
struct Serializer
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serializeVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
    {
        if (schema.current().type != Reflection2::MetaType::TypeStruct)
        {
            return false;
        }
        const int numMembers      = schema.current().numSubAtoms;
        const int structTypeIndex = schema.sourceTypeIndex;
        for (int i = 0; i < numMembers; ++i)
        {
            schema.sourceTypeIndex                                     = structTypeIndex + i + 1;
            SerializerVersionedMemberIterator<BinaryStream, T> visitor = {schema, stream, object,
                                                                          schema.current().order};
            schema.resolveLink();
            Reflection2::MetaClass<T>::visit(visitor);
            if (visitor.consumed)
            {
                if (not visitor.consumedWithSuccess)
                {
                    return false;
                }
            }
            else
            {
                if (not schema.options.allowDropEccessStructMembers)
                    return false;
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
        return Reflection2::MetaClass<T>::visit(SerializerMemberIterator<BinaryStream, T>{stream, object});
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
            IsPrimitive<T>::value && schema.current().type == Reflection2::MetaClass<T>::getMetaType();
        if (isMemcpyable)
        {
            const auto sourceNumBytes = schema.current().size * numSourceItems;
            const auto destNumBytes   = numDestinationItems * sizeof(T);
            const auto minBytes       = min(static_cast<uint32_t>(destNumBytes), sourceNumBytes);
            if (not stream.serialize({object, minBytes}))
                return false;
            if (sourceNumBytes > static_cast<uint32_t>(destNumBytes))
            {
                if (not schema.options.allowDropEccessArrayItems)
                {
                    return false;
                }
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
            if (not schema.options.allowDropEccessArrayItems)
            {
                return false;
            }
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
                if (!Serializer<BinaryStream, T>::serialize(item, stream))
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
        if (not object.resize(sizeInBytes / itemSize))
            return false;

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
            IsPrimitive<T>::value && schema.current().type == Reflection2::MetaClass<T>::getMetaType();
        const size_t   sourceItemSize = schema.current().size;
        const uint32_t numSourceItems = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        if (isMemcpyable)
        {
            if (not object.resizeWithoutInitializing(numSourceItems))
                return false;
        }
        else
        {
            if (not object.resize(numSourceItems))
                return false;
        }
        return SerializerItems<BinaryStream, T>::serializeVersioned(object.data(), stream, schema, numSourceItems,
                                                                    numSourceItems);
    }
};
template <typename T>
struct ClassInfo<SC::Vector<T>>
{
    static constexpr bool IsPacked = false;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("SC::Vector"), HashFor<T>::Hash);
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
            IsPrimitive<T>::value && schema.current().type == Reflection2::MetaClass<T>::getMetaType();

        const size_t   sourceItemSize      = schema.current().size;
        const uint32_t numSourceItems      = static_cast<uint32_t>(sizeInBytes / sourceItemSize);
        const uint32_t numDestinationItems = static_cast<uint32_t>(N);
        if (isMemcpyable)
        {
            if (not object.resizeWithoutInitializing(min(numSourceItems, numDestinationItems)))
                return false;
        }
        else
        {
            if (not object.resize(min(numSourceItems, numDestinationItems)))
                return false;
        }
        return SerializerItems<BinaryStream, T>::serializeItems(object.data(), stream, schema, numSourceItems,
                                                                numDestinationItems);
    }
};

template <typename T, int N>
struct ClassInfo<SC::Array<T, N>>
{
    static constexpr bool IsPacked = false;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("SC::Array"), HashFor<T>::Hash, N);
};

template <typename BinaryStream, typename T>
struct Serializer<BinaryStream, T, typename SC::EnableIf<IsPrimitive<T>::value>::type>
{
    template <typename ValueType>
    [[nodiscard]] static bool readCastValue(T& destination, BinaryStream& stream)
    {
        ValueType value;
        if (not stream.template readAndAdvance<ValueType>(value))
            return false;
        destination = static_cast<T>(value);
        return true;
    }

    [[nodiscard]] static constexpr bool serializeVersioned(T& object, BinaryStream& stream, VersionSchema& schema)
    {

        // clang-format off
        switch (schema.current().type)
        {
            case Reflection2::MetaType::TypeUINT8:      return readCastValue<uint8_t>(object, stream);
            case Reflection2::MetaType::TypeUINT16:     return readCastValue<uint16_t>(object, stream);
            case Reflection2::MetaType::TypeUINT32:     return readCastValue<uint32_t>(object, stream);
            case Reflection2::MetaType::TypeUINT64:     return readCastValue<uint64_t>(object, stream);
            case Reflection2::MetaType::TypeINT8:       return readCastValue<int8_t>(object, stream);
            case Reflection2::MetaType::TypeINT16:      return readCastValue<int16_t>(object, stream);
            case Reflection2::MetaType::TypeINT32:      return readCastValue<int32_t>(object, stream);
            case Reflection2::MetaType::TypeINT64:      return readCastValue<int64_t>(object, stream);
            case Reflection2::MetaType::TypeFLOAT32:
            {
                if(schema.options.allowFloatToIntTruncation || IsSame<T, float>::value || IsSame<T, double>::value)
                {
                    return readCastValue<float>(object, stream);
                }
                return false;
            }
            case Reflection2::MetaType::TypeDOUBLE64:
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
} // namespace Serialization2
} // namespace SC

SC_META2_STRUCT_BEGIN(SC::String)
SC_META2_STRUCT_MEMBER(0, data)
SC_META2_STRUCT_END()

template <typename T, int N>
struct SC::Reflection2::MetaClass<SC::Array<T, N>>
{
    static constexpr MetaType getMetaType() { return MetaType::TypeVector; }
    static constexpr void     build(MetaClassBuilder& builder)
    {
        Atom arrayHeader                   = Atom::create<SC::Array<T, N>>("SC::Array<T, N>");
        arrayHeader.properties.numSubAtoms = 1;
        arrayHeader.properties.setCustomUint32(N);
        builder.atoms.push(arrayHeader);
        builder.atoms.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), TypeToString<T>::get(),
                            &MetaClass<T>::build});
    }
};

template <typename T>
struct SC::Reflection2::MetaClass<SC::Vector<T>>
{
    static constexpr MetaType getMetaType() { return MetaType::TypeVector; }
    static constexpr void     build(MetaClassBuilder& builder)
    {
        Atom vectorHeader                   = Atom::create<SC::Vector<T>>("SC::Vector");
        vectorHeader.properties.numSubAtoms = 1;
        vectorHeader.properties.setCustomUint32(sizeof(T));
        builder.atoms.push(vectorHeader);
        builder.atoms.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), TypeToString<T>::get(),
                            &MetaClass<T>::build});
    }
};

namespace SC
{
namespace Reflection2
{
template <typename Key, typename Value, typename Container>
struct MetaClass<Map<Key, Value, Container>> : MetaStruct<MetaClass<Map<Key, Value, Container>>>
{
    typedef typename SC::Map<Key, Value, Container> T;
    template <typename MemberVisitor>
    static constexpr void visit(MemberVisitor&& builder)
    {
        builder(0, "items", T::items, SC_OFFSET_OF(T, items));
    }
};
} // namespace Reflection2
} // namespace SC

namespace SC
{
struct Serialization2Test;
} // namespace SC

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::PrimitiveStruct)
SC_META2_STRUCT_MEMBER(0, arrayValue)
SC_META2_STRUCT_MEMBER(1, floatValue)
SC_META2_STRUCT_MEMBER(2, int64Value)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::NestedStruct)
SC_META2_STRUCT_MEMBER(0, int16Value)
SC_META2_STRUCT_MEMBER(1, structsArray)
SC_META2_STRUCT_MEMBER(2, doubleVal)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::TopLevelStruct)
SC_META2_STRUCT_MEMBER(0, nestedStruct)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VectorStructSimple)
SC_META2_STRUCT_MEMBER(0, emptyVector)
SC_META2_STRUCT_MEMBER(1, vectorOfInts)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VectorStructComplex)
SC_META2_STRUCT_MEMBER(0, vectorOfStrings)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedStruct1)
SC_META2_STRUCT_MEMBER(2, field2ToRemove)
SC_META2_STRUCT_MEMBER(0, floatValue)
SC_META2_STRUCT_MEMBER(1, fieldToRemove)
SC_META2_STRUCT_MEMBER(3, int64Value)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedStruct2)
SC_META2_STRUCT_MEMBER(3, int64Value)
SC_META2_STRUCT_MEMBER(0, floatValue)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedPoint3D)
SC_META2_STRUCT_MEMBER(0, x)
SC_META2_STRUCT_MEMBER(1, y)
SC_META2_STRUCT_MEMBER(2, z)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedPoint2D)
SC_META2_STRUCT_MEMBER(0, x)
SC_META2_STRUCT_MEMBER(1, y)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedArray1)
SC_META2_STRUCT_MEMBER(0, points)
SC_META2_STRUCT_MEMBER(1, simpleInts)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedArray2)
SC_META2_STRUCT_MEMBER(0, points)
SC_META2_STRUCT_MEMBER(1, simpleInts)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::ConversionStruct1)
SC_META2_STRUCT_MEMBER(0, intToFloat)
SC_META2_STRUCT_MEMBER(1, floatToInt)
SC_META2_STRUCT_MEMBER(2, uint16To32)
SC_META2_STRUCT_MEMBER(3, signed16ToUnsigned)
SC_META2_STRUCT_END()

SC_META2_STRUCT_BEGIN(SC::SerializationTestSuite::ConversionStruct2)
SC_META2_STRUCT_MEMBER(0, intToFloat)
SC_META2_STRUCT_MEMBER(1, floatToInt)
SC_META2_STRUCT_MEMBER(2, uint16To32)
SC_META2_STRUCT_MEMBER(3, signed16ToUnsigned)
SC_META2_STRUCT_END()

// static_assert(SC::Serialization2::ClassInfo<SC::PrimitiveBStruct>().IsPacked, "THIS MUST BE PACKED");
template <typename StreamType>
struct SerializerAdapter
{
    StreamType& stream;
    SerializerAdapter(StreamType& stream) : stream(stream) {}
    template <typename T>
    bool serialize(T& value)
    {
        return SC::Serialization2::Serializer<StreamType, T>::serialize(value, stream);
    }
};

struct SerializerVersionedAdapter
{
    template <typename T, typename StreamType, typename VersionSchema>
    bool serializeVersioned(T& value, StreamType& stream, VersionSchema& versionSchema)
    {
        return SC::Serialization2::Serializer<StreamType, T>::serializeVersioned(value, stream, versionSchema);
    }
};

namespace SC
{
struct Serialization2Test;
}
struct SC::Serialization2Test : public SC::SerializationTestSuite::SerializationTestBase<
                                    SC::Serialization2::BinaryWriterStream,                    //
                                    SC::Serialization2::BinaryReaderStream,                    //
                                    SerializerAdapter<SC::Serialization2::BinaryWriterStream>, //
                                    SerializerAdapter<SC::Serialization2::BinaryReaderStream>>
{
    Serialization2Test(SC::TestReport& report) : SerializationTestBase(report, "Serialization2Test")
    {
        runSameVersionTests();

        runVersionedTests<SC::Reflection2::FlatSchemaCompiler, SerializerVersionedAdapter,
                          SC::Serialization2::VersionSchema>();
    }
};
