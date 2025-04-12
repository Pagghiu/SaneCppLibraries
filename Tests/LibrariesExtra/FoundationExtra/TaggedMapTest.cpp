// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "LibrariesExtra/FoundationExtra/TaggedMap.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"
#include "LibrariesExtra/FoundationExtra/TaggedUnion.h"

namespace SC
{
struct TaggedMapTest;
} // namespace SC

struct SC::TaggedMapTest : public SC::TestCase
{
    TaggedMapTest(SC::TestReport& report) : TestCase(report, "TaggedMapTest")
    {
        using namespace SC;
        if (test_section("basic"))
        {
            basic();
        }
    }

    void basic();
};

//! [TaggedMapTestSnippet]
namespace SC
{
// Create an arbitrary enumeration with some values
struct Compile
{
    enum Type
    {
        libraryPath = 10,
        enableRTTI  = 110,
    };
};

// Create the union definition containing a FieldTypes nested type
struct CompileFlags
{
    // Helper to save some typing
    template <Compile::Type E, typename T>
    using Tag = TaggedType<Compile::Type, E, T>;

    // FieldsTypes MUST be defined to be a TypeList of TaggedType(s)
    using FieldsTypes = TypeTraits::TypeList< // List all associations between type and enumeration
        Tag<Compile::libraryPath, String>,    // Associate Compile::libraryPath with String
        Tag<Compile::enableRTTI, bool>>;      // Associate Compile::enableRTTI with bool type

    using Union = TaggedUnion<CompileFlags>;
};

} // namespace SC

void SC::TaggedMapTest::basic()
{
    TaggedMap<Compile::Type, CompileFlags::Union> taggedMap;

    SC_TEST_EXPECT(taggedMap.get<Compile::libraryPath>() == nullptr);
    *taggedMap.getOrCreate<Compile::libraryPath>() = "My String";
    SC_TEST_EXPECT(*taggedMap.get<Compile::libraryPath>() == "My String");

    SC_TEST_EXPECT(taggedMap.get<Compile::enableRTTI>() == nullptr);
    *taggedMap.getOrCreate<Compile::enableRTTI>() = true;
    SC_TEST_EXPECT(*taggedMap.get<Compile::enableRTTI>());
}
//! [TaggedMapTestSnippet]

namespace SC
{
void runTaggedMapTest(SC::TestReport& report) { TaggedMapTest test(report); }
} // namespace SC
