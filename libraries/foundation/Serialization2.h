#pragma once
#include "Array.h"
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

// TODO: We could probably hardcode these for faster compile time
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
        if (object.size > buffer.size())
            return false;
        numberOfOperations++;
        Span<uint8_t> bytes = object.castTo<uint8_t>();
        memcpy(bytes.data, &buffer[index], bytes.size);
        index += bytes.size;
        return true;
    }
};
} // namespace Serialization2
} // namespace SC
struct SC::Serialization2::VersionSchema
{
    const Span<const Reflection2::MetaProperties> sourceProperties;

    int                         sourceTypeIndex = 0;
    Reflection2::MetaProperties current() const { return sourceProperties.data[sourceTypeIndex]; }
};

template <typename T>
struct SC::Serialization2::ClassInfoMembers
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
struct SC::Serialization2::ClassInfo
{
    static constexpr bool IsPacked = ClassInfoMembers<T>().IsPacked;
    static constexpr auto Hash     = HashFor<T>::Hash;
};

template <typename T, int N>
struct SC::Serialization2::ClassInfo<T[N]>
{
    static constexpr bool IsPacked = ClassInfo<T>::IsPacked;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("[]"), HashFor<T>::Hash, N);
};

template <typename T>
struct SC::Serialization2::ClassInfo<T, typename SC::EnableIf<SC::Serialization2::IsPrimitive<T>::value>::type>
{
    static constexpr bool IsPacked = true;
    static constexpr auto Hash     = HashFor<T>::Hash;
};

template <typename BinaryStream, typename T>
struct SC::Serialization2::SerializerMemberIterator
{
    BinaryStream& stream;
    T&            object;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset) const
    {
        return Serializer<BinaryStream, R>::serialize(object.*member, stream);
    }
};

template <typename BinaryStream, typename T, typename T2>
struct SC::Serialization2::Serializer
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    static constexpr bool serializeVersioned(T& object, BinaryStream& stream, VersionSchema& version)
    {
        SC_TRY_IF(version.current().type == Reflection2::MetaType::TypeStruct);
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

template <typename BinaryStream, typename T, int N>
struct SC::Serialization2::Serializer<BinaryStream, T[N]>
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

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
struct SC::Serialization2::SerializerVector
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serialize(Container& object, BinaryStream& stream)
    {
        uint64_t size = static_cast<uint64_t>(object.size());
        if (not Serializer<BinaryStream, uint64_t>::serialize(size, stream))
            return false;
        if (not object.resize(size))
            return false;

        if (IsItemPacked)
        {
            return stream.serialize({object.data(), sizeof(T) * object.size()});
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
struct SC::Serialization2::Serializer<BinaryStream, SC::Vector<T>>
    : public SerializerVector<BinaryStream, SC::Vector<T>, T>
{
};
template <typename T>
struct SC::Serialization2::ClassInfo<SC::Vector<T>>
{
    static constexpr bool IsPacked = false;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("SC::Vector"), HashFor<T>::Hash);
};

template <typename BinaryStream, typename T, int N>
struct SC::Serialization2::Serializer<BinaryStream, SC::Array<T, N>>
    : public SerializerVector<BinaryStream, SC::Array<T, N>, T>
{
};

template <typename T, int N>
struct SC::Serialization2::ClassInfo<SC::Array<T, N>>
{
    static constexpr bool IsPacked = false;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("SC::Array"), HashFor<T>::Hash, N);
};

template <typename BinaryStream, typename T>
struct SC::Serialization2::Serializer<BinaryStream, T,
                                      typename SC::EnableIf<SC::Serialization2::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(T& object, BinaryStream& stream)
    {
        return stream.serialize({&object, sizeof(T)});
    }
};
SC_META2_STRUCT_BEGIN(SC::String)
SC_META2_STRUCT_MEMBER(0, data)
SC_META2_STRUCT_END()
namespace SC
{
namespace Serialization2
{
struct SimpleBinaryReaderVersioned
{
    template <typename T>
    [[nodiscard]] bool serialize(T& object, Span<const void> source, Span<const Reflection::MetaProperties> properties,
                                 Span<const SC::ConstexprStringView> names)

    {
        return false;
    }
};
} // namespace Serialization2
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
SC_META2_STRUCT_MEMBER(0, floatValue)
SC_META2_STRUCT_MEMBER(1, fieldToRemove)
SC_META2_STRUCT_MEMBER(2, field2ToRemove)
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
        bool res = SC::Serialization2::Serializer<StreamType, T>::serialize(value, stream);
        return res;
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
                                    SerializerAdapter<SC::Serialization2::BinaryReaderStream>, //
                                    SC::Serialization2::SimpleBinaryReaderVersioned,           //
                                    SC::Reflection::FlatSchemaCompiler>
{
    Serialization2Test(SC::TestReport& report) : SerializationTestBase(report, "Serialization2Test")
    {
        runSameVersionTests();
    }
};
