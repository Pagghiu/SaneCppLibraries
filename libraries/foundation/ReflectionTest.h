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
    constexpr bool    operator==(const Member& other) const
    {
        return type == other.type && order == other.order && offset == other.offset && size == other.size &&
               numFields == other.numFields;
    }
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
    constexpr bool operator==(const CompileArray& other) const
    {
        if (size != other.size)
            return false;
        for (int i = 0; i < size; ++i)
        {
            if (not(values[i] == other.values[i]))
                return false;
        }
        return true;
    }
};
constexpr int MAX_NUMBER_OF_MEMBERS = 10;
struct MemberAndName
{
    Member      member;
    const char* name;
    CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> (*getLinkMembers)();

    constexpr MemberAndName() : name(nullptr), getLinkMembers(nullptr) {}
    constexpr MemberAndName(const Member member, const char* name,
                            CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> (*getLinkMembers)())
        : member(member), name(name), getLinkMembers(getLinkMembers)
    {}
    constexpr MemberAndName& operator=(const MemberAndName& other) = default;

    constexpr bool operator==(const MemberAndName& other) const
    {
        const bool sameMember = member == member;
        // TODO: I think that address of getLinkMembers should be the same as we're generating everything during
        // template at call site
        const bool sameLink = getLinkMembers == other.getLinkMembers;
        return sameMember && sameLink;
    }
};
template <typename T>
struct GetDescriptorFor;

template <typename T>
struct GetMembersListFor
{
    static constexpr const MemberAndName*                               get() { return nullptr; }
    static constexpr CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> getLinkMembers()
    {
        return CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS>();
    }
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
    return {Member(get_type<R>(), order, offset, sizeof(R), -1), name, &GetMembersListFor<R>::getLinkMembers};
}
template <typename... Types>
constexpr CompileArray<MemberAndName, sizeof...(Types) + 1> BuildMemberNameArray(MemberAndName structType,
                                                                                 Types... args)
{
    return {MemberAndName(Member(structType.member.type, structType.member.order, structType.member.offset,
                                 structType.member.size, sizeof...(Types)),
                          structType.name, nullptr),
            args...};
}

struct LinkAndIndex
{
    CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> linkMembers;
    int                                                flatternedIndex;
    constexpr LinkAndIndex() : flatternedIndex(0) {}
};

constexpr int count_maximum_links(const MemberAndName* structMember)
{
    int numLinks = 0;
    for (int i = 0; i < structMember->member.numFields; ++i)
    {
        const MemberAndName*                               member      = structMember + 1 + i;
        CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> linkMembers = member->getLinkMembers();

        if (linkMembers.size > 0)
        {
            numLinks += 1 + linkMembers.size;
            numLinks += count_maximum_links(&linkMembers.values[0]);
        }
    }
    return numLinks;
}

template <int N>
constexpr void flattern_links_recursive(const MemberAndName* structMember, CompileArray<LinkAndIndex, N>& collected)
{
    for (int i = 0; i < structMember->member.numFields; ++i)
    {
        const MemberAndName*                               member      = structMember + 1 + i;
        CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> linkMembers = member->getLinkMembers();
        if (linkMembers.size > 0)
        {
            bool found = false;
            for (int searchIDx = 0; searchIDx < collected.size; ++searchIDx)
            {
                if (collected.values[searchIDx].linkMembers == linkMembers)
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
                        const LinkAndIndex& prev = collected.values[collected.size - 1];
                        prevMembers              = prev.linkMembers.values[0].member.numFields;
                    }

                    collected.values[collected.size].linkMembers     = linkMembers;
                    collected.values[collected.size].flatternedIndex = prevMembers;
                    collected.size++;
                }
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
    int prevMembers = 0;
    if (collected.size > 0)
    {
        const LinkAndIndex& prev = collected.values[collected.size - 1];
        prevMembers              = prev.linkMembers.values[0].member.numFields;
    }
    collected.values[collected.size].linkMembers     = inputMembers;
    collected.values[collected.size].flatternedIndex = prevMembers;
    collected.size++;
    return collected;
}

template <int MaxNumParams>
constexpr auto count_members(const CompileArray<LinkAndIndex, MaxNumParams>& links)
{
    const LinkAndIndex& lastLink   = links.values[links.size - 1];
    const auto numCollectedMembers = lastLink.flatternedIndex + lastLink.linkMembers.values[0].member.numFields;
    return numCollectedMembers + links.size;
}

template <int totalMembers, int MaxNumParams>
constexpr auto merge_links_recurse(CompileArray<Member, totalMembers>& members, const LinkAndIndex* currentLink,
                                   const CompileArray<LinkAndIndex, MaxNumParams>& collected)
{
    auto memberAndName             = &currentLink->linkMembers.values[0];
    members.values[members.size++] = memberAndName->member;
    for (int idx = 0; idx < memberAndName->member.numFields; ++idx)
    {
        const MemberAndName* field = memberAndName + 1 + idx;

        members.values[members.size]                                   = field->member;
        CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> linkMembers = field->getLinkMembers();
        if (linkMembers.size > 0)
        {
            for (int findIdx = 0; findIdx < collected.size; ++findIdx)
            {
                if (collected.values[findIdx].linkMembers == linkMembers)
                {
                    members.values[members.size].numFields = collected.values[findIdx].flatternedIndex;
                    break;
                }
            }
        }
        members.size++;
    }
}

template <int totalMembers, int MaxNumParams>
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
    constexpr auto linkMembers  = GetMembersListFor<T>::getLinkMembers();
    constexpr auto maxNumParams = count_maximum_links(linkMembers.values);
    constexpr auto links        = flattern_links<maxNumParams>(linkMembers);
    constexpr auto totalMembers = count_members(links);
    return merge_links<totalMembers>(links);
}

template <typename T>
auto GetLinksFor()
{
    auto linkMembers  = GetMembersListFor<T>::getLinkMembers();
    auto maxNumParams = count_maximum_links(linkMembers.values);
    (void)maxNumParams;
    return flattern_links<8>(linkMembers);
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
struct SC::Reflection::GetMembersListFor<TestNamespace::SimpleStructure>
{
    static constexpr CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> getLinkMembers()
    {
        typedef TestNamespace::SimpleStructure             T;
        CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> asd;
        asd.values[0] = {Member(get_type<T>(), 0, 0, sizeof(T), 2), "SimpleStructure", nullptr};
        asd.values[1] = ReflectField(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        asd.values[2] = ReflectField(1, "f2", &T::f2, SC_OFFSET_OF(T, f2));
        asd.size      = 3;
        return asd;
    }
};

template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::ComplexStructure>
{
    typedef TestNamespace::ComplexStructure                             T;
    static constexpr CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> getLinkMembers()
    {
        CompileArray<MemberAndName, MAX_NUMBER_OF_MEMBERS> asd;
        asd.values[0] = {Member(Type::TypeStruct, 0, 0, sizeof(T), 4), "ComplexStructure", nullptr};
        asd.values[1] = ReflectField(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        asd.values[2] = ReflectField(1, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure));
        asd.values[3] = ReflectField(2, "simpleStructure2", &T::simpleStructure2, SC_OFFSET_OF(T, simpleStructure2));
        asd.values[4] = ReflectField(3, "f4", &T::f1, SC_OFFSET_OF(T, f4));
        asd.size      = 5;
        return asd;
    }
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
