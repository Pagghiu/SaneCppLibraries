#pragma once
#include "InitializerList.h"
#include "Reflection.h"
#include "Test.h"

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
static_assert(sizeof(Member) == 8, "YO watch your step");
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
// constexpr int MAX_NUMBER_OF_MEMBERS = 10;

template <int MAX_MEMBERS>
struct MemberAndName
{
    Member      member;
    const char* name;
    CompileArray<MemberAndName, MAX_MEMBERS> (*getLinkMembers)();

    constexpr MemberAndName() : name(nullptr), getLinkMembers(nullptr) {}
    constexpr MemberAndName(const Member member, const char* name,
                            decltype(MemberAndName::getLinkMembers) getLinkMembers)
        : member(member), name(name), getLinkMembers(getLinkMembers)
    {}
    constexpr MemberAndName& operator=(const MemberAndName& other) = default;
};
template <typename T>
struct GetDescriptorFor;

template <typename T>
struct GetMembersListFor
{
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();
    }
};
// clang-format off
template<typename T> constexpr Type get_type() {return Type::TypeStruct; }
template<> constexpr Type get_type<uint8_t>() { return Type::TypeUINT8; }
template<> constexpr Type get_type<uint16_t>() { return Type::TypeUINT16; }

#define SC_OFFSET_OF(Class, Field) __builtin_offsetof(Class, Field)

// clang-format on

template <int MAX_MEMBERS, typename R, typename T>
constexpr MemberAndName<MAX_MEMBERS> ReflectField(int order, const char* name, R T::*func, size_t offset)
{
    return {Member(get_type<R>(), order, offset, sizeof(R), -1), name,
            &GetMembersListFor<R>::template getLinkMembers<MAX_MEMBERS>};
}
template <int MAX_MEMBERS, typename... Types>
constexpr CompileArray<MemberAndName<MAX_MEMBERS>, sizeof...(Types) + 1> BuildMemberNameArray(
    MemberAndName<MAX_MEMBERS> structType, Types... args)
{
    return {MemberAndName<MAX_MEMBERS>(Member(structType.member.type, structType.member.order, structType.member.offset,
                                              structType.member.size, sizeof...(Types)),
                                       structType.name, nullptr),
            args...};
}

template <int MAX_MEMBERS>
struct LinkAndIndex
{
    CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> linkMembers;
    decltype(linkMembers) (*getLinkMembers)();
    int flatternedIndex;
    constexpr LinkAndIndex() : getLinkMembers(nullptr), flatternedIndex(0) {}
};

template <int MAX_MEMBERS>
constexpr int count_maximum_links(const MemberAndName<MAX_MEMBERS>* structMember)
{
    int numLinks = 0;
    for (int i = 0; i < structMember->member.numFields; ++i)
    {
        const auto* member      = structMember + 1 + i;
        const auto  linkMembers = member->getLinkMembers();

        if (linkMembers.size > 0)
        {
            numLinks += 1 + count_maximum_links(&linkMembers.values[0]);
        }
    }
    return numLinks;
}

template <int MAX_MEMBERS, int N>
constexpr void flattern_links_recursive(const MemberAndName<MAX_MEMBERS>*           structMember,
                                        CompileArray<LinkAndIndex<MAX_MEMBERS>, N>& collected)
{
    for (int i = 0; i < structMember->member.numFields; ++i)
    {
        const auto* member         = structMember + 1 + i;
        auto        linkMembers    = member->getLinkMembers();
        auto        getLinkMembers = member->getLinkMembers;
        if (linkMembers.size > 0)
        {
            bool found = false;
            for (int searchIDx = 0; searchIDx < collected.size; ++searchIDx)
            {
                if (collected.values[searchIDx].getLinkMembers == getLinkMembers)
                {
                    found = true;
                    break;
                }
            }
            if (not found)
            {
                if (linkMembers.size > 0)
                {
                    flattern_links_recursive(linkMembers.values, collected);
                    int prevMembers = 0;
                    if (collected.size > 0)
                    {
                        const auto& prev = collected.values[collected.size - 1];
                        prevMembers      = prev.linkMembers.values[0].member.numFields;
                    }

                    collected.values[collected.size].linkMembers     = linkMembers;
                    collected.values[collected.size].getLinkMembers  = getLinkMembers;
                    collected.values[collected.size].flatternedIndex = prevMembers;
                    collected.size++;
                }
            }
        }
    }
}

template <int MAX_MEMBERS, int MaxNumParams, int ArraySize>
constexpr auto flattern_links(const CompileArray<MemberAndName<MAX_MEMBERS>, ArraySize>& inputMembers)
{
    const auto*                                           rootMember = &inputMembers.values[0];
    CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams> collected;
    flattern_links_recursive(rootMember, collected);
    int prevMembers = 0;
    if (collected.size > 0)
    {
        const auto& prev = collected.values[collected.size - 1];
        prevMembers      = prev.linkMembers.values[0].member.numFields;
    }
    collected.values[collected.size].linkMembers     = inputMembers;
    collected.values[collected.size].getLinkMembers  = rootMember->getLinkMembers;
    collected.values[collected.size].flatternedIndex = prevMembers;
    collected.size++;
    return collected;
}

template <int MAX_MEMBERS, int MaxNumParams>
constexpr auto count_members(const CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams>& links)
{
    const auto& lastLink            = links.values[links.size - 1];
    const auto  numCollectedMembers = lastLink.flatternedIndex + lastLink.linkMembers.values[0].member.numFields;
    return numCollectedMembers + links.size;
}

template <int MAX_MEMBERS, int totalMembers, int MaxNumParams>
constexpr auto merge_links_recurse(CompileArray<Member, totalMembers>&                          members,
                                   const LinkAndIndex<MAX_MEMBERS>*                             currentLink,
                                   const CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams>& collected)
{
    auto memberAndName             = &currentLink->linkMembers.values[0];
    members.values[members.size++] = memberAndName->member;
    for (int idx = 0; idx < memberAndName->member.numFields; ++idx)
    {
        const auto* field = memberAndName + 1 + idx;

        members.values[members.size] = field->member;
        auto linkMembers             = field->getLinkMembers();
        if (linkMembers.size > 0)
        {
            for (int findIdx = 0; findIdx < collected.size; ++findIdx)
            {
                if (collected.values[findIdx].getLinkMembers == field->getLinkMembers)
                {
                    members.values[members.size].numFields = collected.values[findIdx].flatternedIndex;
                    break;
                }
            }
        }
        members.size++;
    }
}

template <int MAX_MEMBERS, int totalMembers, int MaxNumParams>
constexpr auto merge_links(const CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams>& links)
{
    CompileArray<Member, totalMembers> members;
    for (int i = 0; i < links.size; ++i)
    {
        merge_links_recurse(members, &links.values[i], links);
    }
    return members;
}

// You can customize MAX_MEMBERS to match the max number of members (+1) of any descriptor that will be linked
// This is just consuming compile time memory, so it doesn't matter putting any value as long as your compiler
// is able to handle it without running out of heap space :)
template <typename T, int MAX_MEMBERS = 10>
constexpr auto CompileFlatternedDescriptorFor()
{
    constexpr auto linkMembers  = GetMembersListFor<T>::template getLinkMembers<MAX_MEMBERS>();
    constexpr auto maxNumParams = count_maximum_links(linkMembers.values);
    constexpr auto links        = flattern_links<MAX_MEMBERS, maxNumParams>(linkMembers);
    constexpr auto totalMembers = count_members(links);
    return merge_links<MAX_MEMBERS, totalMembers>(links);
}

// template <typename T>
// auto GetLinksFor()
//{
//     auto linkMembers  = GetMembersListFor<T>::getLinkMembers();
//     auto maxNumParams = count_maximum_links(linkMembers.values);
//     (void)maxNumParams;
//     return flattern_links<8>(linkMembers);
// }

#define SC_REFLECT_STRUCT_START(StructName)                                                                            \
    template <>                                                                                                        \
    struct SC::Reflection::GetMembersListFor<StructName>                                                               \
    {                                                                                                                  \
        typedef StructName T;                                                                                          \
        template <int MAX_MEMBERS>                                                                                     \
        static constexpr auto getLinkMembers()                                                                         \
        {                                                                                                              \
            CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;                                              \
            fields.values[fields.size++] = {Member(Type::TypeStruct, 0, 0, sizeof(T), 0), #StructName, nullptr};

#define SC_REFLECT_STRUCT_END()                                                                                        \
    fields.values[0].member.numFields = fields.size - 1;                                                               \
    return fields;                                                                                                     \
    }                                                                                                                  \
    }                                                                                                                  \
    ;

#define SC_REFLECT_FIELD(Order, Field)                                                                                 \
    fields.values[fields.size++] = ReflectField<MAX_MEMBERS>(Order, #Field, &T::Field, SC_OFFSET_OF(T, Field));

} // namespace Reflection
} // namespace SC

#if 0

template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::SimpleStructure>
{
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        typedef TestNamespace::SimpleStructure                T;
        CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;
        fields.values[fields.size++]      = {Member(get_type<T>(), 0, 0, sizeof(T), 2), "SimpleStructure", nullptr};
        fields.values[fields.size++]      = ReflectField<MAX_MEMBERS>(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        fields.values[fields.size++]      = ReflectField<MAX_MEMBERS>(1, "f2", &T::f2, SC_OFFSET_OF(T, f2));
        fields.values[0].member.numFields = fields.size - 1;
        return fields;
    }
};

template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::ComplexStructure>
{
    typedef TestNamespace::ComplexStructure T;
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;
        fields.values[fields.size++] = {Member(Type::TypeStruct, 0, 0, sizeof(T), 4), "ComplexStructure", nullptr};
        fields.values[fields.size++] = ReflectField<MAX_MEMBERS>(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        fields.values[fields.size++] =
            ReflectField<MAX_MEMBERS>(1, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure));
        fields.values[fields.size++] =
            ReflectField<MAX_MEMBERS>(2, "simpleStructure2", &T::simpleStructure2, SC_OFFSET_OF(T, simpleStructure2));
        fields.values[fields.size++]      = ReflectField<MAX_MEMBERS>(3, "f4", &T::f1, SC_OFFSET_OF(T, f4));
        fields.values[0].member.numFields = fields.size - 1;
        return fields;
    }
};
#else

SC_REFLECT_STRUCT_START(TestNamespace::SimpleStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, f2)
SC_REFLECT_STRUCT_END()

SC_REFLECT_STRUCT_START(TestNamespace::ComplexStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, simpleStructure)
SC_REFLECT_FIELD(2, simpleStructure2)
SC_REFLECT_FIELD(3, f4)
SC_REFLECT_STRUCT_END()

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
            auto ComplexStructureLinks = GetLinksFor<TestNamespace::ComplexStructure>();
            for (int i = 0; i < ComplexStructureLinks.size; ++i)
            {
                auto val = ComplexStructureLinks.values[i];
                Console::c_printf("Link name=%s flatternedIndex=%d numMembers=%d\n", val.linkMembers.values[0].name,
                                  val.flatternedIndex, val.linkMembers.values[0].member.numFields);
            }
#endif
            constexpr auto MyCompileTimeDescriptor = CompileFlatternedDescriptorFor<TestNamespace::ComplexStructure>();
            printMembersFlat(MyCompileTimeDescriptor.values);
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
