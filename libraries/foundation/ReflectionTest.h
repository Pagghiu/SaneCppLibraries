#pragma once
#include "Reflection.h"
#include "Test.h"

namespace TestNamespace
{
struct SimpleStructure
{
    // Base Types
    SC::uint8_t  f1  = 0;
    SC::uint16_t f2  = 1;
    SC::uint32_t f3  = 2;
    SC::uint64_t f4  = 3;
    SC::int8_t   f5  = 4;
    SC::int16_t  f6  = 5;
    SC::int32_t  f7  = 6;
    SC::int64_t  f8  = 7;
    float        f9  = 8;
    double       f10 = 9;
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

    // Template Types
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
    int16_t  numFields; // 2

    constexpr Member() : type(Type::TypeInvalid), order(0), offset(0), size(0), numFields(0)
    {
        static_assert(sizeof(Member) == 8, "Size must be 8 bytes");
    }
    constexpr Member(Type type, uint8_t order, uint16_t offset, uint16_t size, int16_t numFields)
        : type(type), order(order), offset(offset), size(size), numFields(numFields)
    {}
    constexpr void    setLinkIndex(int16_t linkIndex) { numFields = linkIndex; }
    constexpr int16_t getLinkIndex() const { return numFields; }
};

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
};

// clang-format off
struct EmptyGetListMembers { template <int MAX_MEMBERS> static constexpr auto getLinkMembers(){ return CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>(); }};
template <typename T> struct GetMembersListFor : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeInvalid;}};

template <> struct GetMembersListFor<uint8_t>  : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeUINT8;}};
template <> struct GetMembersListFor<uint16_t> : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeUINT16;}};
template <> struct GetMembersListFor<uint32_t> : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeUINT32;}};
template <> struct GetMembersListFor<uint64_t> : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeUINT64;}};
template <> struct GetMembersListFor<int8_t>   : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeINT8;}};
template <> struct GetMembersListFor<int16_t>  : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeINT16;}};
template <> struct GetMembersListFor<int32_t>  : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeINT32;}};
template <> struct GetMembersListFor<int64_t>  : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeINT64;}};
template <> struct GetMembersListFor<float>    : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeFLOAT;}};
template <> struct GetMembersListFor<double>   : public EmptyGetListMembers {static constexpr Type getMemberType(){return Type::TypeDOUBLE;}};
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

template <int MAX_MEMBERS, int MAX_POSSIBLE_LINKS>
constexpr int count_unique_links(const CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>& rootStruct)
{
    CompileArray<CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>, MAX_POSSIBLE_LINKS> structsQueue;
    structsQueue.values[structsQueue.size++] = rootStruct;
    CompileArray<typename MemberAndName<MAX_MEMBERS>::GetLinkMemberFunction, MAX_POSSIBLE_LINKS> alreadyVisitedLinks;

    int numLinks = 1;

    while (structsQueue.size > 0)
    {
        structsQueue.size--;
        const auto structMembers = structsQueue.values[structsQueue.size]; // MUST copy as we're modifying queue
        for (int idx = 0; idx < structMembers.values[0].member.numFields; ++idx)
        {
            const auto& member = structMembers.values[idx + 1];

            bool alreadyVisitedLink = false;
            for (int searchIDX = 0; searchIDX < alreadyVisitedLinks.size; ++searchIDX)
            {
                if (alreadyVisitedLinks.values[searchIDX] == member.getLinkMembers)
                {
                    alreadyVisitedLink = true;
                    break;
                }
            }
            if (not alreadyVisitedLink)
            {
                alreadyVisitedLinks.values[alreadyVisitedLinks.size++] = member.getLinkMembers;
                const auto linkMembers                                 = member.getLinkMembers();
                if (member.member.type == Type::TypeInvalid)
                {
                    return -1; // Missing descriptor for type
                }
                else if (linkMembers.size > 0)
                {
                    numLinks++;
                    structsQueue.values[structsQueue.size++] = linkMembers;
                }
                else if (member.member.type == Type::TypeStruct)
                {
                    return -2; // Somebody created a struct with empty list of members
                }
            }
        }
    }
    return numLinks;
}

template <int MAX_MEMBERS, int UNIQUE_LINKS_NUMBER, int MAX_POSSIBLE_LINKS>
constexpr auto find_all_links(const CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>& inputMembers,
                              typename MemberAndName<MAX_MEMBERS>::GetLinkMemberFunction   rootMemberFunction)
{
    CompileArray<LinkAndIndex<MAX_MEMBERS>, UNIQUE_LINKS_NUMBER>                               links;
    CompileArray<CompileArray<MemberAndName<MAX_MEMBERS>, MAX_MEMBERS>, MAX_POSSIBLE_LINKS> structsQueue;
    structsQueue.values[structsQueue.size++] = inputMembers;
    links.values[links.size].getLinkMembers  = rootMemberFunction;
    links.values[links.size].flatternedIndex = 0;
    links.size++;
    while (structsQueue.size > 0)
    {
        structsQueue.size--;
        const auto rootStruct = structsQueue.values[structsQueue.size]; // MUST be copy (we modify structsQueue)
        for (int memberIDX = 0; memberIDX < rootStruct.values[0].member.numFields; ++memberIDX)
        {
            const auto& member         = rootStruct.values[memberIDX + 1];
            auto        getLinkMembers = member.getLinkMembers;
            bool        foundLink      = false;
            for (int searchIDX = 0; searchIDX < links.size; ++searchIDX)
            {
                if (links.values[searchIDX].getLinkMembers == getLinkMembers)
                {
                    foundLink = true;
                    break;
                }
            }
            if (not foundLink)
            {
                auto linkMembers = member.getLinkMembers();
                if (linkMembers.size > 0)
                {
                    structsQueue.values[structsQueue.size++] = linkMembers;
                    int prevMembers                          = 0;
                    if (links.size > 0)
                    {
                        const auto& prev = links.values[links.size - 1];
                        prevMembers      = prev.flatternedIndex + prev.getLinkMembers().size;
                    }

                    links.values[links.size].getLinkMembers  = getLinkMembers;
                    links.values[links.size].flatternedIndex = prevMembers;
                    links.size++;
                }
            }
        }
    }
    return links;
}

template <int MAX_MEMBERS, int totalMembers, int MAX_LINKS_NUMBER>
constexpr void merge_links_flat(const CompileArray<LinkAndIndex<MAX_MEMBERS>, MAX_LINKS_NUMBER>& links,
                                CompileArray<Member, totalMembers>&                              mergedMembers,
                                CompileArray<const char*, totalMembers>*                         mergedNames)
{
    for (int linkIndex = 0; linkIndex < links.size; ++linkIndex)
    {
        auto linkMembers                           = links.values[linkIndex].getLinkMembers();
        mergedMembers.values[mergedMembers.size++] = linkMembers.values[0].member;
        if (mergedNames)
        {
            mergedNames->values[mergedNames->size++] = linkMembers.values[0].name;
        }
        for (int memberIndex = 0; memberIndex < linkMembers.values[0].member.numFields; ++memberIndex)
        {
            const auto& field                        = linkMembers.values[1 + memberIndex];
            mergedMembers.values[mergedMembers.size] = field.member;
            if (mergedNames)
            {
                mergedNames->values[mergedNames->size++] = field.name;
            }
            for (int findIdx = 0; findIdx < links.size; ++findIdx)
            {
                if (links.values[findIdx].getLinkMembers == field.getLinkMembers)
                {
                    mergedMembers.values[mergedMembers.size].setLinkIndex(links.values[findIdx].flatternedIndex);
                    break;
                }
            }
            mergedMembers.size++;
        }
    }
}

template <int TOTAL_MEMBERS>
struct FlatternedDescriptor
{
    CompileArray<Member, TOTAL_MEMBERS>      members;
    CompileArray<const char*, TOTAL_MEMBERS> names;
};

// You can customize MAX_MEMBERS to match the max number of members (+1) of any descriptor that will be linked
// You can customize MAX_POSSIBLE_LINKS for the max numer of unique types in the system
// This is just consuming compile time memory, so it doesn't matter putting any value as long as your compiler
// is able to handle it without running out of heap space :)
template <typename T, int MAX_MEMBERS = 20, int MAX_POSSIBLE_LINKS = 500>
constexpr auto CompileFlatternedDescriptorFor()
{
    constexpr auto linkMembers = GetMembersListFor<T>::template getLinkMembers<MAX_MEMBERS>();
    static_assert(linkMembers.size > 0, "Missing Descriptor for root class");
    constexpr auto UNIQUE_LINKS_NUMBER = count_unique_links<MAX_MEMBERS, MAX_POSSIBLE_LINKS>(linkMembers);
    static_assert(UNIQUE_LINKS_NUMBER >= 0, "Missing Descriptor for a class reachable by root class");
    constexpr auto links = find_all_links<MAX_MEMBERS, UNIQUE_LINKS_NUMBER, MAX_POSSIBLE_LINKS>(
        linkMembers, &GetMembersListFor<T>::template getLinkMembers<MAX_MEMBERS>);
    constexpr auto totalMembers =
        links.values[links.size - 1].flatternedIndex + links.values[links.size - 1].getLinkMembers().size;
    FlatternedDescriptor<totalMembers> result;
    merge_links_flat(links, result.members, &result.names);
    return result;
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

        const uint16_t lowN          = N & 0xffff;
        const uint16_t highN         = (N >> 16) & 0xffff;
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
SC_REFLECT_FIELD(2, f3)
SC_REFLECT_FIELD(3, f4)
SC_REFLECT_FIELD(4, f5)
SC_REFLECT_FIELD(5, f6)
SC_REFLECT_FIELD(6, f7)
SC_REFLECT_FIELD(7, f8)
SC_REFLECT_FIELD(8, f9)
SC_REFLECT_FIELD(9, f10)
SC_REFLECT_STRUCT_END()

SC_REFLECT_STRUCT_START(TestNamespace::IntermediateStructure)
// SC_REFLECT_FIELD(0, vectorOfInt)
SC_REFLECT_FIELD(1, simpleStructure)
SC_REFLECT_STRUCT_END()

SC_REFLECT_STRUCT_START(TestNamespace::ComplexStructure)
// SC_REFLECT_FIELD(0, f1)
// SC_REFLECT_FIELD(1, simpleStructure)
// SC_REFLECT_FIELD(2, simpleStructure2)
// SC_REFLECT_FIELD(3, f4)
SC_REFLECT_FIELD(4, intermediateStructure)
// SC_REFLECT_FIELD(5, vectorOfSimpleStruct)
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
            // count_unique_links<10>(GetMembersListFor<TestNamespace::ComplexStructure>::template
            // getLinkMembers<10>());
            // SC_RELEASE_ASSERT(numlinks == 3);
            constexpr auto MyCompileTimeDescriptor = CompileFlatternedDescriptorFor<TestNamespace::ComplexStructure>();
            printMembersFlat(MyCompileTimeDescriptor.members.values, MyCompileTimeDescriptor.names.values);
        }
    }
    template <int NumMembers>
    void printMembersFlat(const Reflection::Member (&member)[NumMembers], const char* const (&names)[NumMembers])
    {
        int currentMembers = 0;
        while (currentMembers < NumMembers)
        {
            currentMembers += printMembers(currentMembers, member + currentMembers, names + currentMembers, 0) + 1;
        }
    }

    int printMembers(int currentMemberIDX, const Reflection::Member* member, const char* const* memberName,
                     int indentation)
    {
        using namespace SC;
        using namespace SC::Reflection;
        SC_RELEASE_ASSERT(member->type == Reflection::Type::TypeStruct || member->type >= Reflection::Type::TypeVector);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("[LinkIndex=%d] %s (%d fields)\n", currentMemberIDX, *memberName, member->numFields);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("{\n");
        for (int idx = 0; idx < member->numFields; ++idx)
        {
            auto&       field     = member[idx + 1];
            const char* fieldName = memberName[idx + 1];
            for (int i = 0; i < indentation + 1; ++i)
                Console::c_printf("\t");
            Console::c_printf("Type=%d\tOffset=%d\tSize=%d\tName=%s", (int)field.type, field.offset,
                              field.size, fieldName);
            if (field.getLinkIndex() >= 0)
            {
                Console::c_printf("\t[LinkIndex=%d]", field.getLinkIndex());
            }
            Console::c_printf("\n");
        }
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("}\n");
        return member->numFields;
    }
};
