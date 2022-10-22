#pragma once
#include "Reflection.h"
#include "Test.h"

namespace SC
{
struct ReflectionTestEmbedding;

namespace ReflectionEmbedding
{
enum class Type : uint8_t
{
    TypeInvalid = 0,
    TypeStruct  = 1,
    TypeUINT8   = 2,
    TypeUINT16  = 3,
    TypeUINT32  = 4,
    TypeUINT64  = 5,
    TypeINT8    = 6,
    TypeINT16   = 7,
    TypeINT32   = 8,
    TypeINT64   = 9,
    TypeFLOAT   = 10,
    TypeDOUBLE  = 11,
};

struct Member
{
    Type     type;      // 1
    uint8_t  order;     // 1
    uint16_t offset;    // 2
    uint16_t size;      // 2
    int16_t  numFields; // 2
    constexpr Member() : type(Type::TypeInvalid), order(0), offset(0), size(0), numFields(0) {}
    constexpr Member(Type type, uint8_t order, uint16_t offset, uint16_t size, int16_t numFields)
        : type(type), order(order), offset(offset), size(size), numFields(numFields)
    {}
    constexpr Member& operator=(const Member& other) = default;
};
static_assert(sizeof(Member) == 8, "YO");
struct SimpleStructure
{
    // Base Types
    uint8_t  f1 = 0;
    uint16_t f2 = 1;
    //    uint32_t f3  = 2;
    //    uint64_t f4  = 3;
    //    int8_t   f5  = 4;
    //    int16_t  f6  = 5;
    //    int32_t  f7  = 6;
    //    int64_t  f8  = 7;
    //    float    f9  = 8;
    //    double   f10 = 9;
};

struct ComplexStructure
{
    uint8_t         f1 = 0;
    SimpleStructure simpleStructure;
    SimpleStructure simpleStructure2;
    uint16_t        f4 = 0;
};

template <typename T, typename... Ts>
struct Tuple
{
    constexpr Tuple(const T& t, const Ts&... ts) : value(t), rest(ts...) {}
    constexpr int size() const { return 1 + rest.size(); }

    T            value;
    Tuple<Ts...> rest;
};

template <typename T>
struct Tuple<T>
{
    constexpr Tuple(const T& t) : value(t) {}
    constexpr int size() const { return 1; }

    T value;
};

template <typename... Types>
constexpr Tuple<Types...> make_tuple(Types... types)
{
    return Tuple<Types...>(types...);
}

template <typename T>
struct TypeAndName
{
    T           type;
    const char* name;
};

template <typename T>
constexpr auto make_type_name(T type, const char* name) -> TypeAndName<T>
{
    return TypeAndName<T>{type, name};
}

template <typename... Types>
constexpr auto build_struct(Types... t) -> decltype(make_tuple(make_tuple(t.type...), make_tuple(t.name...)))
{
    return make_tuple(make_tuple(t.type...), make_tuple(t.name...));
}

template <typename T>
struct ReflectStruct;

template <>
struct ReflectStruct<uint8_t>
{
    static constexpr Member get(uint8_t order, uint16_t offset)
    {
        return Member(Type::TypeUINT8, order, offset, sizeof(uint8_t), 0);
    }
};
template <>
struct ReflectStruct<uint16_t>
{
    static constexpr Member get(uint8_t order, uint16_t offset)
    {
        return Member(Type::TypeUINT16, order, offset, sizeof(uint16_t), 0);
    }
};

constexpr auto SimpleStructureData = build_struct(
    make_type_name(ReflectStruct<decltype(SimpleStructure::f1)>::get(0, __builtin_offsetof(SimpleStructure, f1)), "f1"),
    make_type_name(ReflectStruct<decltype(SimpleStructure::f2)>::get(1, __builtin_offsetof(SimpleStructure, f2)),
                   "f2"));

template <>
struct ReflectStruct<SimpleStructure>
{
    static constexpr decltype(SimpleStructureData)& getStruct() { return SimpleStructureData; }
    static constexpr Tuple<Member, decltype(decltype(SimpleStructureData)::value)> get(uint8_t order, uint16_t offset)
    {
        // TODO: Figure out a way to compute num members
        return {Member(Type::TypeStruct, order, offset, sizeof(SimpleStructure), 2), getStruct().value};
    }
    static constexpr int getHash() { return 1; }
};
static constexpr auto ComplexStructureData = build_struct(
    make_type_name(ReflectStruct<decltype(ComplexStructure::f1)>::get(0, __builtin_offsetof(ComplexStructure, f1)),
                   "f1"),
    make_type_name(ReflectStruct<decltype(ComplexStructure::simpleStructure)>::get(
                       1, __builtin_offsetof(ComplexStructure, simpleStructure)),
                   "simpleStructure"),
    make_type_name(ReflectStruct<decltype(ComplexStructure::simpleStructure2)>::get(
                       2, __builtin_offsetof(ComplexStructure, simpleStructure2)),
                   "simpleStructure2"),
    make_type_name(ReflectStruct<decltype(ComplexStructure::f4)>::get(0, __builtin_offsetof(ComplexStructure, f4)),
                   "f4"));
template <>
struct ReflectStruct<ComplexStructure>
{
    static constexpr decltype(ComplexStructureData)& getStruct() { return ComplexStructureData; }
    static constexpr Tuple<Member, decltype(decltype(ComplexStructureData)::value)> get(uint8_t order, uint16_t offset)
    {
        return {Member(Type::TypeStruct, order, offset, sizeof(ComplexStructure), 4), getStruct().value};
    }
    static constexpr int getHash() { return 2; }
};

} // namespace ReflectionEmbedding
} // namespace SC

struct SC::ReflectionTestEmbedding : public SC::TestCase
{
    ReflectionTestEmbedding(SC::TestReport& report) : TestCase(report, "ReflectionTestEmbedding")
    {
        using namespace SC;
        using namespace SC::ReflectionEmbedding;

        if (test_section("normal"))
        {
            static_assert(ReflectStruct<SimpleStructure>::getStruct().rest.value.value[0] == 'f', "asd");
            print<SimpleStructure>();
            print<ComplexStructure>();
            auto* asd = &ReflectStruct<SimpleStructure>::getStruct();
            Console::c_printf("%p\n", asd);
            constexpr auto csdata = ReflectStruct<ComplexStructure>::getStruct().value;
            static_assert(csdata.value.order == 0, "");
            static_assert(csdata.rest.value.value.order == 1, "");
            static_assert(sizeof(csdata) == sizeof(Member) * 8, "");
            auto vv = ReflectStruct<ComplexStructure>::get(0, 0);
            //            auto val = vv.value;
            const Member* member = &vv.value; //&csdata.value;
            printMembers(member, 0);
            //            const char* const* fieldNames =
            //            &ReflectStruct<ComplexStructure>::getStruct().rest.value.value;
            //            for (size_t idx = 0; idx < 4;++idx)
            //            {
            //                Console::c_printf("[%lu] Type=%d Offset=%d\n", idx,  (int)member[idx].type,
            //                member[idx].offset);
            //            }
        }
    }

    size_t printMembers(const ReflectionEmbedding::Member* member, int indentation)
    {
        using namespace SC;
        using namespace SC::ReflectionEmbedding;
        SC_RELEASE_ASSERT(member->type == ReflectionEmbedding::Type::TypeStruct);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("Struct (numMembers=%d)\n", member->numFields);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("{\n");
        size_t fieldsToSkip = 0;
        for (size_t idx = 0; idx < member->numFields; ++idx)
        {
            auto& mem = member[idx + 1 + fieldsToSkip];
            for (int i = 0; i < indentation + 1; ++i)
                Console::c_printf("\t");
            Console::c_printf("[%lu] Type=%d Offset=%d Size=%d", idx, (int)mem.type, mem.offset, mem.size);
            if (mem.numFields >= 0)
            {
                Console::c_printf(" linkID=%d", mem.numFields);
            }
            Console::c_printf("\n");
            if (mem.type == ReflectionEmbedding::Type::TypeStruct)
            {
                fieldsToSkip += printMembers(&mem, indentation + 1);
            }
        }
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("}\n");
        return fieldsToSkip + member->numFields;
    }

    template <typename T>
    void print()
    {
        using namespace SC;
        using namespace SC::ReflectionEmbedding;

        const size_t       numFields  = ReflectStruct<T>::getStruct().rest.value.size();
        const char* const* fieldNames = &ReflectStruct<T>::getStruct().rest.value.value;
        for (size_t idx = 0; idx < numFields; ++idx)
        {
            const char* fieldName = fieldNames[idx];
            Console::printUTF8(StringView(fieldName, strlen(fieldName), true));
            Console::printUTF8("\n"_sv);
        }
    }
};
