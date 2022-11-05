#pragma once
#include "Array.h"
#include "Reflection.h"
#include "String.h"
#include "Test.h"
#include "Vector.h"

namespace SC
{
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
// template <typename T> struct IsArray   : false_type  {};
// template <typename T, int N> struct IsArray<T[N]>   : true_type  {typedef T type;};
// clang-format on

namespace Serialization
{
template <typename Writer, typename T, typename T2 = void>
struct Serializer;
template <typename Writer, typename T>
struct SerializerMemberIterator;

template <typename T>
struct IsPackedMembers;

template <typename T, typename T2 = void>
struct IsPacked;

template <typename Writer, typename Container, typename T>
struct SerializerVector;

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
} // namespace Serialization
} // namespace SC

template <typename T>
struct SC::Serialization::IsPackedMembers
{
    size_t memberSizeSum = 0;
    bool   isPacked      = false;

    constexpr IsPackedMembers()
    {
        if (Reflection::MetaClass<T>::members(*this))
        {
            isPacked = memberSizeSum == sizeof(T);
        }
    }

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset)
    {
        if (not IsPacked<R>().isPacked)
        {
            return false;
        }
        memberSizeSum += sizeof(R);
        return true;
    }
};

template <typename T, typename T2>
struct SC::Serialization::IsPacked
{
    static constexpr bool isPacked = IsPackedMembers<T>().isPacked;
};

template <typename T, int N>
struct SC::Serialization::IsPacked<T[N]>
{
    static constexpr bool isPacked = IsPacked<T>().isPacked;
};

template <typename T>
struct SC::Serialization::IsPacked<T, typename SC::EnableIf<SC::IsPrimitive<T>::value>::type>
{
    static constexpr bool isPacked = true;
};

template <typename Writer, typename T>
struct SC::Serialization::SerializerMemberIterator
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
struct SC::Serialization::Serializer
{
    static constexpr bool               isItemPacked = IsPacked<T>().isPacked;
    [[nodiscard]] static constexpr bool serialize(T& object, Writer& writer)
    {
        if (isItemPacked)
        {
            return writer.serialize({&object, sizeof(T)});
        }
        return Reflection::MetaClass<T>::members(SerializerMemberIterator<Writer, T>{writer, object});
    }
};

template <typename Writer, typename T, int N>
struct SC::Serialization::Serializer<Writer, T[N]>
{
    static constexpr bool               isItemPacked = IsPacked<T>().isPacked;
    [[nodiscard]] static constexpr bool serialize(T (&object)[N], Writer& writer)
    {
        if (isItemPacked)
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
struct SC::Serialization::SerializerVector
{
    static constexpr bool               isItemPacked = IsPacked<T>().isPacked;
    [[nodiscard]] static constexpr bool serialize(Container& object, Writer& writer)
    {
        uint64_t size = static_cast<uint64_t>(object.size());
        if (not Serializer<Writer, uint64_t>::serialize(size, writer))
            return false;
        if (not object.resize(size))
            return false;

        if (isItemPacked)
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
struct SC::Serialization::Serializer<Writer, SC::Vector<T>> : public SerializerVector<Writer, SC::Vector<T>, T>
{
};
// TODO: We shouldn't need to write these for custom types
template <typename T>
struct SC::Serialization::IsPacked<SC::Vector<T>>
{
    static constexpr bool isPacked = false;
};

template <typename Writer, typename T, int N>
struct SC::Serialization::Serializer<Writer, SC::Array<T, N>> : public SerializerVector<Writer, SC::Array<T, N>, T>
{
};

// TODO: We shouldn't need to write these for custom types
template <typename T, int N>
struct SC::Serialization::IsPacked<SC::Array<T, N>>
{
    static constexpr bool isPacked = false;
};

template <typename Writer, typename T>
struct SC::Serialization::Serializer<Writer, T, typename SC::EnableIf<SC::IsPrimitive<T>::value>::type>
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
SC_META_STRUCT_BEGIN(SC::PrimitiveBStruct)
SC_META_STRUCT_MEMBER(0, arrayValue)
SC_META_STRUCT_MEMBER(1, floatValue)
SC_META_STRUCT_MEMBER(2, int64Value)
SC_META_STRUCT_MEMBER(3, vector)
SC_META_STRUCT_END()
// static_assert(SC::Serialization::IsPacked<SC::PrimitiveBStruct>().isPacked, "THIS MUST BE PACKED");

struct SC::SerializationBTest : public SC::TestCase
{
    SerializationBTest(SC::TestReport& report) : TestCase(report, "SerializationBTest")
    {
        using namespace SC;
        using namespace SC::Serialization;
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
