#pragma once
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
struct IntermediateStructure
{
    SC::Vector<int> vectorOfInt;
    SimpleStructure simpleStructure;
};

struct ComplexStructure
{
    SC::uint8_t                 f1 = 0;
    SimpleStructure             simpleStructure;
    SimpleStructure             simpleStructure2;
    SC::uint16_t                f4 = 0;
    IntermediateStructure       intermediateStructure;
    SC::Vector<SimpleStructure> vectorOfSimpleStruct;
};
} // namespace TestNamespace

namespace SC
{
struct ReflectionTest;

namespace Reflection
{

enum class Type : uint8_t
{
    // Primitive Types
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
    // Custom Types
    TypeArray  = 20,
    TypeVector = 21,
    TypeMap    = 22,
};

struct Member
{
    Type     type;      // 1
    uint8_t  order;     // 1
    uint16_t offset;    // 2
    uint16_t size;      // 2
    int16_t  numFields; // 2 // This is also linkID when part of a struct. Cannot use unions because of constexpr

    constexpr Member() : type(Type::TypeInvalid), order(0), offset(0), size(0), numFields(0) {}
    constexpr Member(Type type, uint8_t order, uint16_t offset, uint16_t size, int16_t numFields)
        : type(type), order(order), offset(offset), size(size), numFields(numFields)
    {}
    constexpr void    setLinkID(int16_t linkID) { numFields = linkID; }
    constexpr int16_t getLinkID() const { return numFields; }
};
static_assert(sizeof(Member) == 8, "YO watch your step");
template <typename T, int N>
struct CompileArray
{
    T   values[N] = {};
    int size      = 0;
};

template <int MAX_MEMBERS>
struct MemberAndName
{
    typedef CompileArray<MemberAndName, MAX_MEMBERS> (*GetLinkMemberFunction)();

    Member                member;
    const char*           name;
    GetLinkMemberFunction getLinkMembers;

    constexpr MemberAndName() : name(nullptr), getLinkMembers(nullptr) {}
    constexpr MemberAndName(const Member member, const char* name,
                            decltype(MemberAndName::getLinkMembers) getLinkMembers)
        : member(member), name(name), getLinkMembers(getLinkMembers)
    {}
    constexpr MemberAndName& operator=(const MemberAndName& other) = default;
};

template <typename T>
struct GetMembersListFor
{
    static constexpr Type getMemberType() { return Type::TypeInvalid; }
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();
    }
};

// clang-format off
template <> struct GetMembersListFor<uint8_t>
{
    static constexpr Type getMemberType(){return Type::TypeUINT8;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
template <> struct GetMembersListFor<uint16_t>
{
    static constexpr Type getMemberType(){return Type::TypeUINT16;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
template <> struct GetMembersListFor<uint32_t>
{
    static constexpr Type getMemberType(){return Type::TypeUINT32;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
template <> struct GetMembersListFor<uint64_t>
{
    static constexpr Type getMemberType(){return Type::TypeUINT64;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
template <> struct GetMembersListFor<int8_t>
{
    static constexpr Type getMemberType(){return Type::TypeINT8;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
template <> struct GetMembersListFor<int16_t>
{
    static constexpr Type getMemberType(){return Type::TypeINT16;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
template <> struct GetMembersListFor<int32_t>
{
    static constexpr Type getMemberType(){return Type::TypeINT32;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
template <> struct GetMembersListFor<int64_t>
{
    static constexpr Type getMemberType(){return Type::TypeINT64;}
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>();}
};
// clang-format on

template <int MAX_MEMBERS, typename R, typename T>
constexpr MemberAndName<MAX_MEMBERS> ReflectField(int order, const char* name, R T::*, size_t offset)
{
    return {Member(GetMembersListFor<R>::getMemberType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
            name, &GetMembersListFor<R>::template getLinkMembers<MAX_MEMBERS>};
}

template <int MAX_MEMBERS, typename... Types>
constexpr CompileArray<MemberAndName<MAX_MEMBERS>, sizeof...(Types) + 1> BuildMemberNameArray(
    MemberAndName<MAX_MEMBERS> structType, Types... args)
{
    CompileArray<MemberAndName<MAX_MEMBERS>, sizeof...(Types) + 1> res;
    res.values[res.size++] =
        MemberAndName<MAX_MEMBERS>(Member(structType.member.type, structType.member.order, structType.member.offset,
                                          structType.member.size, sizeof...(Types)),
                                   structType.name, nullptr);
    auto argsArr = {args...};
    for (auto a : argsArr)
    {
        res.values[res.size++] = a;
    }
    return res;
}

template <int MAX_MEMBERS>
struct LinkAndIndex
{
    typename MemberAndName<MAX_MEMBERS>::GetLinkMemberFunction getLinkMembers;
    int                                                        flatternedIndex;
    constexpr LinkAndIndex() : getLinkMembers(nullptr), flatternedIndex(0) {}
};

template <int MAX_MEMBERS>
constexpr int count_maximum_links(const CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>& structMember)
{
    CompileArray<CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>, MAX_MEMBERS> queue;
    queue.values[queue.size++] = structMember;

    int numLinks = 1;

    while (queue.size > 0)
    {
        queue.size--;
        const auto rootMembers = queue.values[queue.size];
        for (int idx = 0; idx < rootMembers.values[0].member.numFields; ++idx)
        {
            const auto& member      = rootMembers.values[idx + 1];
            const auto  linkMembers = member.getLinkMembers();
            if (member.member.type == Type::TypeInvalid)
            {
                return -1; // Missing descriptor for type
            }
            else if (linkMembers.size > 0)
            {
                numLinks++;
                queue.values[queue.size++] = linkMembers;
            }
        }
    }
    return numLinks;
}

template <int MAX_MEMBERS, int MaxNumParams, int N>
constexpr void flattern_links_iterative(
    CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>                   rootMembers,
    CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams>&                  collected,
    CompileArray<CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>, N>& toRecurse)
{

    for (int i = 0; i < rootMembers.values[0].member.numFields; ++i)
    {
        const auto& member      = rootMembers.values[i + 1];
        auto        linkMembers = member.getLinkMembers();
        if (linkMembers.size > 0)
        {
            auto getLinkMembers = member.getLinkMembers;
            bool found          = false;
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
                toRecurse.values[toRecurse.size++] = linkMembers;
                int prevMembers                    = 0;
                if (collected.size > 0)
                {
                    const auto& prev = collected.values[collected.size - 1];
                    prevMembers      = prev.flatternedIndex + prev.getLinkMembers().size;
                }

                collected.values[collected.size].getLinkMembers  = getLinkMembers;
                collected.values[collected.size].flatternedIndex = prevMembers;
                collected.size++;
            }
        }
    }
}

template <int MAX_MEMBERS, int MaxNumParams, int ArraySize>
constexpr auto flattern_links(const CompileArray<MemberAndName<MAX_MEMBERS>, ArraySize>& inputMembers,
                              typename MemberAndName<MAX_MEMBERS>::GetLinkMemberFunction lastMember)
{
    CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams>                            collected;
    CompileArray<CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>, MAX_MEMBERS> toRecurse;
    toRecurse.values[toRecurse.size++] = inputMembers;
    while (toRecurse.size > 0)
    {
        toRecurse.size--;
        flattern_links_iterative(toRecurse.values[toRecurse.size], collected, toRecurse);
    }
    int prevMembers = 0;
    if (collected.size > 0)
    {
        const auto& prev = collected.values[collected.size - 1];
        prevMembers      = prev.flatternedIndex + prev.getLinkMembers().size;
    }
    collected.values[collected.size].getLinkMembers  = lastMember;
    collected.values[collected.size].flatternedIndex = prevMembers;
    collected.size++;
    return collected;
}

template <int MAX_MEMBERS, int MaxNumParams>
constexpr auto count_members(const CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams>& links)
{
    const auto& lastLink = links.values[links.size - 1];
    return lastLink.flatternedIndex + lastLink.getLinkMembers().size;
}

template <int MAX_MEMBERS, int totalMembers, int MaxNumParams>
constexpr auto merge_links_iterative(CompileArray<Member, totalMembers>&                          members,
                                     const LinkAndIndex<MAX_MEMBERS>*                             currentLink,
                                     const CompileArray<LinkAndIndex<MAX_MEMBERS>, MaxNumParams>& collected)
{
    auto linkMembers               = currentLink->getLinkMembers();
    members.values[members.size++] = linkMembers.values[0].member;
    for (int idx = 0; idx < linkMembers.values[0].member.numFields; ++idx)
    {
        const auto& field = linkMembers.values[1 + idx];

        members.values[members.size] = field.member;
        auto linkMembers             = field.getLinkMembers();
        if (linkMembers.size > 0)
        {
            for (int findIdx = 0; findIdx < collected.size; ++findIdx)
            {
                if (collected.values[findIdx].getLinkMembers == field.getLinkMembers)
                {
                    members.values[members.size].setLinkID(collected.values[findIdx].flatternedIndex);
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
        merge_links_iterative(members, &links.values[i], links);
    }
    return members;
}

// You can customize MAX_MEMBERS to match the max number of members (+1) of any descriptor that will be linked
// This is just consuming compile time memory, so it doesn't matter putting any value as long as your compiler
// is able to handle it without running out of heap space :)
template <typename T, int MAX_MEMBERS = 10>
constexpr auto CompileFlatternedDescriptorFor()
{
    constexpr auto linkMembers = GetMembersListFor<T>::template getLinkMembers<MAX_MEMBERS>();
    static_assert(linkMembers.size > 0, "Missing Descriptor for root class");
    constexpr auto maxNumParams = count_maximum_links<MAX_MEMBERS>(linkMembers);
    static_assert(maxNumParams >= 0, "Missing Descriptor for a class reachable by root class");
    constexpr auto links = flattern_links<MAX_MEMBERS, maxNumParams, MAX_MEMBERS>(
        linkMembers, &GetMembersListFor<T>::template getLinkMembers<MAX_MEMBERS>);
    constexpr auto totalMembers = count_members(links);
    return merge_links<MAX_MEMBERS, totalMembers>(links);
}

#define SC_REFLECT_STRUCT_START(StructName)                                                                            \
    template <>                                                                                                        \
    struct SC::Reflection::GetMembersListFor<StructName>                                                               \
    {                                                                                                                  \
        typedef StructName    T;                                                                                       \
        static constexpr Type getMemberType() { return Type::TypeStruct; }                                             \
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

template <typename T, SC::size_t N>
struct SC::Reflection::GetMembersListFor<SC::Array<T, N>>
{
    static constexpr Type getMemberType() { return Type::TypeArray; }
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;
        const uint16_t                                        lowN  = N & 0xffff;
        const uint16_t                                        highN = (N >> 16) & 0xffff;
        fields.values[fields.size++] = {Member(getMemberType(), lowN, highN, sizeof(SC::Array<T, N>), 0), "SC::Array",
                                        nullptr};
        fields.values[fields.size++] = {Member(GetMembersListFor<T>::getMemberType(), 0, 0, sizeof(T), -1), "T",
                                        &GetMembersListFor<T>::template getLinkMembers<MAX_MEMBERS>};
        fields.values[0].member.numFields = fields.size - 1;
        return fields;
    }
};

template <typename T>
struct SC::Reflection::GetMembersListFor<SC::Vector<T>>
{
    static constexpr Type getMemberType() { return Type::TypeVector; }
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;
        fields.values[fields.size++] = {Member(getMemberType(), 0, 0, sizeof(SC::Vector<T>), 0), "SC::Vector", nullptr};
        fields.values[fields.size++] = {Member(GetMembersListFor<T>::getMemberType(), 0, 0, sizeof(T), -1), "T",
                                        &GetMembersListFor<T>::template getLinkMembers<MAX_MEMBERS>};
        fields.values[0].member.numFields = fields.size - 1;
        return fields;
    }
};

#if 0

template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::SimpleStructure>
{
    static constexpr Type getMemberType() { return Type::TypeStruct; }
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        typedef TestNamespace::SimpleStructure                T;
        CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;
        fields.values[fields.size++]      = {Member(Type::TypeStruct, 0, 0, sizeof(T), 2), "SimpleStructure", nullptr};
        fields.values[fields.size++]      = ReflectField<MAX_MEMBERS>(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        fields.values[fields.size++]      = ReflectField<MAX_MEMBERS>(1, "f2", &T::f2, SC_OFFSET_OF(T, f2));
        fields.values[0].member.numFields = fields.size - 1;
        return fields;
    }
};
template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::IntermediateStructure>
{
    static constexpr Type getMemberType() { return Type::TypeStruct; }
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        typedef TestNamespace::IntermediateStructure          T;
        CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;
        fields.values[fields.size++] = {Member(Type::TypeStruct, 0, 0, sizeof(T), 0), "IntermediateStructure", nullptr};
        fields.values[fields.size++] =
            ReflectField<MAX_MEMBERS>(0, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure));
        fields.values[fields.size++] =
            ReflectField<MAX_MEMBERS>(1, "vectorOfInt", &T::vectorOfInt, SC_OFFSET_OF(T, vectorOfInt));
        fields.values[0].member.numFields = fields.size - 1;
        return fields;
    }
};
template <>
struct SC::Reflection::GetMembersListFor<TestNamespace::ComplexStructure>
{

    static constexpr Type getMemberType() { return Type::TypeStruct; }
    template <int MAX_MEMBERS>
    static constexpr auto getLinkMembers()
    {
        typedef TestNamespace::ComplexStructure               T;
        CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS> fields;
        fields.values[fields.size++] = {Member(Type::TypeStruct, 0, 0, sizeof(T), 4), "ComplexStructure", nullptr};
        fields.values[fields.size++] = ReflectField<MAX_MEMBERS>(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        fields.values[fields.size++] =
            ReflectField<MAX_MEMBERS>(1, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure));
        fields.values[fields.size++] =
            ReflectField<MAX_MEMBERS>(2, "simpleStructure2", &T::simpleStructure2, SC_OFFSET_OF(T, simpleStructure2));
        fields.values[fields.size++] = ReflectField<MAX_MEMBERS>(3, "f4", &T::f1, SC_OFFSET_OF(T, f4));
        fields.values[fields.size++] = ReflectField<MAX_MEMBERS>(4, "intermediate", &T::intermediateStructure,
                                                                 SC_OFFSET_OF(T, intermediateStructure));
        fields.values[fields.size++] = ReflectField<MAX_MEMBERS>(5, "vectorOfSimpleStruct", &T::vectorOfSimpleStruct,
                                                                 SC_OFFSET_OF(T, vectorOfSimpleStruct));
        fields.values[0].member.numFields = fields.size - 1;
        return fields;
    }
};
#else

SC_REFLECT_STRUCT_START(TestNamespace::SimpleStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, f2)
SC_REFLECT_STRUCT_END()

SC_REFLECT_STRUCT_START(TestNamespace::IntermediateStructure)
SC_REFLECT_FIELD(0, vectorOfInt)
SC_REFLECT_FIELD(1, simpleStructure)
SC_REFLECT_STRUCT_END()

SC_REFLECT_STRUCT_START(TestNamespace::ComplexStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, simpleStructure)
SC_REFLECT_FIELD(2, simpleStructure2)
SC_REFLECT_FIELD(3, f4)
SC_REFLECT_FIELD(4, intermediateStructure)
SC_REFLECT_FIELD(4, vectorOfSimpleStruct)
SC_REFLECT_STRUCT_END()

#endif

struct SC::ReflectionTest : public SC::TestCase
{
    ReflectionTest(SC::TestReport& report) : TestCase(report, "ReflectionTest")
    {
        using namespace SC;
        using namespace SC::Reflection;
        if (test_section("Print Complex structure"))
        {
            // auto numlinks =
            // count_maximum_links<10>(GetMembersListFor<TestNamespace::ComplexStructure>::template
            // getLinkMembers<10>());
            // SC_RELEASE_ASSERT(numlinks == 3);
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

    int printMembers(const Reflection::Member* member, int indentation)
    {
        using namespace SC;
        using namespace SC::Reflection;
        SC_RELEASE_ASSERT(member->type == Reflection::Type::TypeStruct || member->type >= Reflection::Type::TypeVector);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("Struct (numMembers=%d)\n", member->numFields);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("{\n");
        int fieldsToSkip = 0;
        for (int idx = 0; idx < member->numFields; ++idx)
        {
            auto& mem = member[idx + 1 + fieldsToSkip];
            for (int i = 0; i < indentation + 1; ++i)
                Console::c_printf("\t");
            Console::c_printf("[%d] Type=%d Offset=%d Size=%d", idx, (int)mem.type, mem.offset, mem.size);
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
