// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/VectorMap.h"
#include "Libraries/Containers/Array.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct VectorMapTest;
}

struct SC::VectorMapTest : public SC::TestCase
{
    VectorMapTest(SC::TestReport& report) : TestCase(report, "VectorMapTest")
    {
        using namespace SC;
        if (test_section("contains"))
        {
            VectorMap<int, int> map;
            SC_TEST_EXPECT(map.insertIfNotExists({1, 2}));
            SC_TEST_EXPECT(map.insertIfNotExists({2, 3}));
            const int* value;
            SC_TEST_EXPECT(map.contains(1, value) && *value == 2);
            SC_TEST_EXPECT(map.contains(2, value) && *value == 3);
            SC_TEST_EXPECT(map.contains(2, value) && *value == 3);
            SC_TEST_EXPECT(not map.contains(3));
        }
        if (test_section("array"))
        {
            VectorMap<String, String, Array<VectorMapItem<String, String>, 2>> map;
            SC_TEST_EXPECT(map.insertIfNotExists({"Ciao", "Fra"}));
            SC_TEST_EXPECT(map.insertIfNotExists({"Bella", "Bro"}));
            SC_TEST_EXPECT(not map.insertIfNotExists({"Fail", "Fail"}));
            const String* value;
            SC_TEST_EXPECT(map.contains("Ciao", value) && *value == "Fra");
            SC_TEST_EXPECT(map.contains("Bella", value) && *value == "Bro");
        }
        if (test_section("get"))
        {
            VectorMap<String, String, Array<VectorMapItem<String, String>, 2>> map;
            SC_TEST_EXPECT(map.insertIfNotExists({"Ciao", "Fra"}));
            SC_TEST_EXPECT(map.insertIfNotExists({"Bella", "Bro"}));
            String* result1 = map.get("Ciao");
            SC_TEST_EXPECT(result1 and result1->view() == "Fra");
            auto result2 = map.get("Fail");
            SC_TEST_EXPECT(result2 == nullptr);
            SC_TEST_EXPECT(*map.get("Bella") == "Bro");
        }
        if (test_section("StrongID"))
        {
            struct Key
            {
                using ID = StrongID<Key>;
            };
            VectorMap<Key::ID, String> map;

            const Key::ID key1 = Key::ID::generateUniqueKey(map);
            SC_TEST_EXPECT(map.insertIfNotExists({key1, "key1"}));
            auto res = map.insertValueUniqueKey("key2");
            SC_TEST_EXPECT(res);
            const Key::ID key2 = *res;
            const Key::ID key3 = Key::ID::generateUniqueKey(map);
            SC_TEST_EXPECT(map.get(key1)->view() == "key1");
            SC_TEST_EXPECT(map.get(key2)->view() == "key2");
            SC_TEST_EXPECT(not map.get(key3));
        }
    }
    bool vectorMapSnippet();
};

bool SC::VectorMapTest::vectorMapSnippet()
{
    //! [VectorMapSnippet]
    VectorMap<String, int> map;
    SC_TRY(map.insertIfNotExists({"A", 2})); // Allocates a String
    SC_TRY(map.insertIfNotExists({"B", 3})); // Allocates a String
    const int* value;
    SC_TRY(map.contains("A", value) && *value == 2); // <-- "A" is a StringView, avoiding allocation
    SC_TRY(map.contains("B", value) && *value == 3); // <-- "B" is a StringView, avoiding allocation
    SC_TRY(not map.contains("C"));                   // <-- "C" is a StringView, avoiding allocation
    //! [VectorMapSnippet]
    return true;
}

namespace SC
{
void runVectorMapTest(SC::TestReport& report) { VectorMapTest test(report); }
} // namespace SC
