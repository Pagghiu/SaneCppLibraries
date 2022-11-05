#pragma once
#include "Array.h"
#include "Reflection2.h"
#include "String.h"
#include "Test.h"
#include "Vector.h"

namespace SC
{
namespace Serialization2
{
template <typename Writer, typename T, typename T2 = void>
struct Serializer;
template <typename Writer, typename T>
struct SerializerMemberIterator;

template <typename T>
struct ClassInfoMembers;

template <typename T, typename T2 = void>
struct ClassInfo;

template <typename Writer, typename Container, typename T>
struct SerializerVector;

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
// clang-format on

struct BinaryWriter
{
    size_t              index = 0;
    SC::Vector<uint8_t> buffer;
    int                 numOperations = 0;

    [[nodiscard]] bool serialize(Span<const void> object)
    {
        numOperations++;
        Span<const uint8_t> bytes = object.castTo<const uint8_t>();
        return buffer.appendCopy(bytes.data, bytes.size);
    }
};

struct BinaryReader
{
    size_t              index = 0;
    SC::Vector<uint8_t> buffer;
    int                 numOperations = 0;

    [[nodiscard]] bool serialize(Span<void> object)
    {
        if (object.size > buffer.size())
            return false;
        numOperations++;
        Span<uint8_t> bytes = object.castTo<uint8_t>();
        memcpy(bytes.data, &buffer[index], bytes.size);
        index += bytes.size;
        return true;
    }
};
} // namespace Serialization2
} // namespace SC

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
    static constexpr auto Hash     = Reflection2::MetaClass<T>::Hash;
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

template <typename Writer, typename T>
struct SC::Serialization2::SerializerMemberIterator
{
    Writer& writer;
    T&      object;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset) const
    {
        return Serializer<Writer, R>::serialize(object.*member, writer);
    }
};

template <typename Writer, typename T, typename T2>
struct SC::Serialization2::Serializer
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serialize(T& object, Writer& writer)
    {
        if (IsItemPacked)
        {
            return writer.serialize({&object, sizeof(T)});
        }
        return Reflection2::MetaClass<T>::visit(SerializerMemberIterator<Writer, T>{writer, object});
    }
};

template <typename Writer, typename T, int N>
struct SC::Serialization2::Serializer<Writer, T[N]>
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serialize(T (&object)[N], Writer& writer)
    {
        if (IsItemPacked)
        {
            return writer.serialize({object, sizeof(object)});
        }
        else
        {
            for (auto& item : object)
            {
                if (!Serializer<Writer, T>::serialize(item, writer))
                    return false;
            }
            return true;
        }
    }
};

template <typename Writer, typename Container, typename T>
struct SC::Serialization2::SerializerVector
{
    static constexpr bool IsItemPacked = ClassInfo<T>::IsPacked;

    [[nodiscard]] static constexpr bool serialize(Container& object, Writer& writer)
    {
        uint64_t size = static_cast<uint64_t>(object.size());
        if (not Serializer<Writer, uint64_t>::serialize(size, writer))
            return false;
        if (not object.resize(size))
            return false;

        if (IsItemPacked)
        {
            return writer.serialize({object.data(), sizeof(T) * object.size()});
        }
        else
        {
            for (auto& item : object)
            {
                if (!Serializer<Writer, T>::serialize(item, writer))
                    return false;
            }
            return true;
        }
    }
};

template <typename Writer, typename T>
struct SC::Serialization2::Serializer<Writer, SC::Vector<T>> : public SerializerVector<Writer, SC::Vector<T>, T>
{
};
template <typename T>
struct SC::Serialization2::ClassInfo<SC::Vector<T>>
{
    static constexpr bool IsPacked = false;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("SC::Vector"), HashFor<T>::Hash);
};

template <typename Writer, typename T, int N>
struct SC::Serialization2::Serializer<Writer, SC::Array<T, N>> : public SerializerVector<Writer, SC::Array<T, N>, T>
{
};

template <typename T, int N>
struct SC::Serialization2::ClassInfo<SC::Array<T, N>>
{
    static constexpr bool IsPacked = false;
    static constexpr auto Hash     = SC::CombineHash(SC::StringHash("SC::Array"), HashFor<T>::Hash, N);
};

template <typename Writer, typename T>
struct SC::Serialization2::Serializer<Writer, T, typename SC::EnableIf<SC::Serialization2::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(T& object, Writer& writer)
    {
        return writer.serialize({&object, sizeof(T)});
    }
};

namespace SC
{
struct SerializationBTest;
struct PrimitiveBStruct;
} // namespace SC

struct SC::PrimitiveBStruct
{
    uint8_t     arrayValue[4] = {0, 1, 2, 3};
    float       floatValue    = 1.5f;
    int64_t     int64Value    = -13;
    Vector<int> vector        = {1, 2, 3};

    bool operator!=(const PrimitiveBStruct& other) const
    {
        for (int i = 0; i < ConstantArraySize(arrayValue); ++i)
            if (arrayValue[i] != other.arrayValue[i])
                return true;
        if (floatValue != other.floatValue)
            return true;
        if (int64Value != other.int64Value)
            return true;
        if (vector.size() != other.vector.size())
            return false;
        for (int i = 0; i < vector.size(); ++i)
        {
            if (vector[i] != other.vector[i])
                return true;
        }
        return false;
    }
};
SC_META2_STRUCT_BEGIN(SC::PrimitiveBStruct)
SC_META2_STRUCT_MEMBER(0, arrayValue)
SC_META2_STRUCT_MEMBER(1, floatValue)
SC_META2_STRUCT_MEMBER(2, int64Value)
SC_META2_STRUCT_MEMBER(3, vector)
SC_META2_STRUCT_END()
// static_assert(SC::Serialization2::ClassInfo<SC::PrimitiveBStruct>().IsPacked, "THIS MUST BE PACKED");

struct SC::SerializationBTest : public SC::TestCase
{
    SerializationBTest(SC::TestReport& report) : TestCase(report, "SerializationBTest")
    {
        using namespace SC;
        using namespace SC::Serialization2;
        if (test_section("Primitive Structure Write"))
        {
            PrimitiveBStruct struct1;
            BinaryWriter     writer;
            bool             res;

            res = Serializer<BinaryWriter, PrimitiveBStruct>::serialize(struct1, writer);
            SC_TEST_EXPECT(res);
            SC_TEST_EXPECT(writer.numOperations == 5);
            BinaryReader reader;
            reader.buffer = move(writer.buffer);
            PrimitiveBStruct struct2;
            memset(&struct2, 0, sizeof(struct2));
            res = Serializer<BinaryReader, PrimitiveBStruct>::serialize(struct2, reader);
            SC_TEST_EXPECT(res);
            SC_TEST_EXPECT(reader.numOperations == writer.numOperations);
            SC_TEST_EXPECT(not(struct1 != struct2));
        }
    }
};
