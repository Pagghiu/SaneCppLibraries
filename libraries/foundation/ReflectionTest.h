#pragma once
#include "Reflection.h"
#include "Test.h"
#include "InitializerList.h"

namespace TestNamespace
{
struct SimpleStructure
{
    // Base Types
    SC::uint8_t  f1 = 0;
    SC::uint16_t f2 = 1;
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
    SC::uint8_t     f1 = 0;
    SimpleStructure simpleStructure;
    SimpleStructure simpleStructure2;
    SC::uint16_t    f4 = 0;
};
} // namespace TestNamespace

namespace SC
{
struct ReflectionTest;

namespace Reflection
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
template <typename T, int N>
struct CompileArray
{
    typedef T type;
    T         values[N];
    int       size;
    constexpr CompileArray()
    {
        for (int i = 0; i < N; ++i)
            values[i] = T();
        size = 0;
    }
    constexpr CompileArray(std::initializer_list<T> ilist)
    {
        size = 0;
        for (auto i : ilist)
        {
            values[size++] = i;
        }
    }
};

struct MemberAndName
{
    Member               member;
    const char*          name;
    const MemberAndName* link;

    constexpr MemberAndName() : name(nullptr), link(nullptr) {}
    constexpr MemberAndName(const Member member, const char* name, const MemberAndName* link)
        : member(member), name(name), link(link)
    {}
    constexpr MemberAndName& operator=(const MemberAndName& other) = default;
};
template <typename T>
struct GetDescriptorFor;

template <typename T>
struct GetMembersListFor
{
    static constexpr const MemberAndName* get() { return nullptr; }
};
// clang-format off
template<typename T> constexpr Type get_type() {return Type::TypeStruct; }
template<> constexpr Type get_type<uint8_t>() { return Type::TypeUINT8; }
template<> constexpr Type get_type<uint16_t>() { return Type::TypeUINT16; }

#define SC_OFFSET_OF(Class, Field) __builtin_offsetof(Class, Field)

// clang-format on

template <typename R, typename T>
constexpr MemberAndName ReflectField(int order, const char* name, R T::*func, size_t offset)
{
    return {Member(get_type<R>(), order, offset, sizeof(R), -1), name, GetMembersListFor<R>::get()};
}
template <typename... Types>
constexpr CompileArray<MemberAndName, sizeof...(Types) + 1> BuildMemberNameArray(MemberAndName structType,
                                                                                 Types... args)
{

    return {MemberAndName(Member(structType.member.type, structType.member.order, structType.member.offset,
                                 structType.member.size, sizeof...(Types)),
                          structType.name, structType.link),
            args...};
}

struct LinkAndIndex
{
    const MemberAndName* link;
    int                  flatternedIndex;
    constexpr LinkAndIndex(const MemberAndName* link, int flatternedIndex)
        : link(link), flatternedIndex(flatternedIndex)
    {}
    constexpr LinkAndIndex() : link(nullptr), flatternedIndex(0) {}
};

constexpr int count_maximum_links(const MemberAndName* structMember)
{
    int numLinks = 0;
    for (int i = 0; i < structMember->member.numFields; ++i)
    {
        const MemberAndName* member = structMember + 1 + i;
        if (member->link != nullptr && member->link->member.numFields > 0)
        {
            numLinks += 1 + count_maximum_links(member->link);
        }
    }
    return numLinks;
}

template <int N>
constexpr void flattern_links_recursive(const MemberAndName* structMember, CompileArray<LinkAndIndex, N>& collected)
{
    for (int i = 0; i < structMember->member.numFields; ++i)
    {
        const MemberAndName* member = structMember + 1 + i;
        if (member->link != nullptr && member->link->member.numFields > 0)
        {
            bool found = false;
            for (int searchIDx = 0; searchIDx < collected.size; ++searchIDx)
            {
                if (collected.values[searchIDx].link == member->link)
                {
                    found = true;
                    break;
                }
            }
            if (not found)
            {
                flattern_links_recursive(member->link, collected);
                const auto prevMembers = collected.size > 0
                                             ? collected.values[collected.size - 1].flatternedIndex +
                                                   collected.values[collected.size - 1].link->member.numFields
                                             : 0;

                collected.values[collected.size++] = {member->link, prevMembers};
            }
        }
    }
}

template <int MaxNumParams, int ArraySize>
constexpr auto flattern_links(const CompileArray<MemberAndName, ArraySize>& inputMembers)
{
    const MemberAndName*                     rootMember = &inputMembers.values[0];
    CompileArray<LinkAndIndex, MaxNumParams> collected;
    flattern_links_recursive(rootMember, collected);
    const auto prevMembers = collected.size > 0 ? collected.values[collected.size - 1].flatternedIndex +
                                                      collected.values[collected.size - 1].link->member.numFields
                                                : 0;

    collected.values[collected.size++] = {rootMember, prevMembers};
    return collected;
}

template <int MaxNumParams>
constexpr auto count_members(const CompileArray<LinkAndIndex, MaxNumParams>& links)
{
    const LinkAndIndex& lastLink            = links.values[links.size - 1];
    const auto          numCollectedMembers = lastLink.flatternedIndex + lastLink.link->member.numFields;
    return numCollectedMembers + links.size;
}

template <int MaxNumParams, int totalMembers>
constexpr auto merge_links_recurse(CompileArray<Member, totalMembers>& members, const LinkAndIndex* currentLink,
                                   const CompileArray<LinkAndIndex, MaxNumParams>& collected)
{
    auto memberAndName             = currentLink->link;
    members.values[members.size++] = memberAndName->member;
    for (int idx = 0; idx < memberAndName->member.numFields; ++idx)
    {
        const MemberAndName* field = memberAndName + 1 + idx;

        members.values[members.size] = field->member;
        if (field->link != nullptr)
        {
            for (int findIdx = 0; findIdx < collected.size; ++findIdx)
            {
                if (collected.values[findIdx].link == field->link)
                {
                    members.values[members.size].numFields = collected.values[findIdx].flatternedIndex;
                    break;
                }
            }
        }
        members.size++;
    }
}

template <int MaxNumParams, int totalMembers>
constexpr auto merge_links(const CompileArray<LinkAndIndex, MaxNumParams>& links)
{
    CompileArray<Member, totalMembers> members;
    for (int i = 0; i < links.size; ++i)
    {
        merge_links_recurse(members, &links.values[i], links);
    }
    return members;
}

template <typename T>
constexpr auto CompileFlatternedDescriptorFor()
{
    constexpr auto maxNumParams = count_maximum_links(GetMembersListFor<T>::get());
    constexpr auto links        = flattern_links<maxNumParams>(GetMembersListFor<T>::getDescriptor());
    constexpr auto totalMembers = count_members<maxNumParams>(links);
    return merge_links<maxNumParams, totalMembers>(links);
}

#define SC_REFLECT_STRUCT_START(StructName)                                                                            \
    template <>                                                                                                        \
    struct SC::Reflection::GetDescriptorFor<StructName>                                                                \
    {                                                                                                                  \
        typedef StructName    T;                                                                                       \
        static constexpr auto get()                                                                                    \
        {                                                                                                              \
return SC::Reflection::BuildMemberNameArray({SC::Reflection::Member(Type::TypeStruct, 0, 0, sizeof(T), 0), #StructName, nullptr}

#define SC__REFLECT_STRUCT_END_IMPL2(StructName, Counter)                                                              \
);                                                                                                                     \
    }                                                                                                                  \
    }                                                                                                                  \
    ;                                                                                                                  \
    namespace SC                                                                                                       \
    {                                                                                                                  \
    namespace Reflection                                                                                               \
    {                                                                                                                  \
    constexpr auto Members##Counter = SC::Reflection::GetDescriptorFor<StructName>::get();                             \
    }                                                                                                                  \
    }                                                                                                                  \
    template <>                                                                                                        \
    struct SC::Reflection::GetMembersListFor<StructName>                                                               \
    {                                                                                                                  \
        static constexpr const auto* get() { return Members##Counter.values; }                                         \
        static constexpr auto&       getDescriptor() { return Members##Counter; }                                      \
    };
#define SC__REFLECT_STRUCT_END_IMPL1(StructName, Counter) SC__REFLECT_STRUCT_END_IMPL2(StructName, Counter)

#define SC_REFLECT_STRUCT_END(StructName) SC__REFLECT_STRUCT_END_IMPL1(StructName, __COUNTER__)

#define SC_REFLECT_FIELD(Order, Field) , ReflectField(Order, #Field, &T::Field, SC_OFFSET_OF(T, Field))

} // namespace Reflection
} // namespace SC

#if 1

template <>
struct SC::Reflection::GetDescriptorFor<TestNamespace::SimpleStructure>
{

    typedef TestNamespace::SimpleStructure T;
    static constexpr auto                  get()
    {
        return BuildMemberNameArray({Member(get_type<T>(), 0, 0, sizeof(T), 2), "SimpleStructure", nullptr},
                                    ReflectField(0, "f1", &T::f1, SC_OFFSET_OF(T, f1)),
                                    ReflectField(1, "f2", &T::f2, SC_OFFSET_OF(T, f2)));
    }
};

namespace SC
{
namespace Reflection
{
constexpr auto Members0 = SC::Reflection::GetDescriptorFor<TestNamespace::SimpleStructure>::get();
}
} // namespace SC
template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::SimpleStructure>
{
    static constexpr const MemberAndName* get() { return Members0.values; }
    static constexpr auto&                getDescriptor() { return Members0; }
};

template <>
struct SC::Reflection::GetDescriptorFor<TestNamespace::ComplexStructure>
{
    typedef TestNamespace::ComplexStructure T;

    static constexpr auto get()
    {
        return BuildMemberNameArray(
            {Member(Type::TypeStruct, 0, 0, sizeof(T), 0), "ComplexStructure", nullptr},
            ReflectField(0, "f1", &T::f1, SC_OFFSET_OF(T, f1)),
            ReflectField(1, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure)),
            ReflectField(2, "simpleStructure2", &T::simpleStructure2, SC_OFFSET_OF(T, simpleStructure2)),
            ReflectField(3, "f4", &T::f1, SC_OFFSET_OF(T, f4)));
    }
};
namespace SC
{
namespace Reflection
{
constexpr auto Members1 = SC::Reflection::GetDescriptorFor<TestNamespace::ComplexStructure>::get();
}
} // namespace SC

template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::ComplexStructure>
{
    static constexpr const MemberAndName* get() { return Members1.values; }
    static constexpr auto&                getDescriptor() { return Members1; }
};
#else

SC_REFLECT_STRUCT_START(TestNamespace::SimpleStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, f2)
SC_REFLECT_STRUCT_END(TestNamespace::SimpleStructure)

SC_REFLECT_STRUCT_START(TestNamespace::ComplexStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, simpleStructure)
SC_REFLECT_FIELD(2, simpleStructure2)
SC_REFLECT_FIELD(3, f4)
SC_REFLECT_STRUCT_END(TestNamespace::ComplexStructure)

#endif

struct SC::ReflectionTest : public SC::TestCase
{
    ReflectionTest(SC::TestReport& report) : TestCase(report, "ReflectionTest")
    {
        using namespace SC;
        using namespace SC::Reflection;
        if (test_section("ASDF"))
        {
#if 0
            for (int i = 0; i < ComplexStructureLinks.size; ++i)
            {
                auto val = ComplexStructureLinks.values[i];
                Console::c_printf("Link name=%s flatternedIndex=%d numMembers=%d\n", val.link->name,
                                  val.flatternedIndex, val.link->member.numFields);
            }
#endif
            printMembersFlat(CompileFlatternedDescriptorFor<TestNamespace::ComplexStructure>().values);
        }
    }
    template <int NumMembers>
    void printMembersFlat(const Reflection::Member (&member)[NumMembers])
    {
        int currentMembers = 0;
        while (currentMembers < NumMembers)
        {
            currentMembers += printMembers(&member[currentMembers], 0) + 1;
        }
    }

    size_t printMembers(const Reflection::Member* member, int indentation)
    {
        using namespace SC;
        using namespace SC::Reflection;
        SC_RELEASE_ASSERT(member->type == Reflection::Type::TypeStruct);
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
        }
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("}\n");
        return fieldsToSkip + member->numFields;
    }
};
