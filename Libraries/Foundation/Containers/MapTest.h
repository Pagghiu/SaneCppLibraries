// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "../Containers/Array.h"
#include "../Language/StrongID.h"
#include "../Strings/String.h"
#include "Map.h"

namespace SC
{
struct MapTest;
}

struct SC::MapTest : public SC::TestCase
{
    MapTest(SC::TestReport& report) : TestCase(report, "MapTest")
    {
        using namespace SC;
        if (test_section("contains"))
        {
            Map<int, int> map;
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
            Map<String, String, Array<MapItem<String, String>, 2>> map;
            SC_TEST_EXPECT(map.insertIfNotExists({"Ciao"_a8, "Fra"_a8}));
            SC_TEST_EXPECT(map.insertIfNotExists({"Bella"_a8, "Bro"_a8}));
            SC_TEST_EXPECT(not map.insertIfNotExists({"Fail"_a8, "Fail"_a8}));
            const String* value;
            SC_TEST_EXPECT(map.contains("Ciao"_a8, value) && *value == "Fra");
            SC_TEST_EXPECT(map.contains("Bella"_a8, value) && *value == "Bro");
        }
        if (test_section("get"))
        {
            Map<String, String, Array<MapItem<String, String>, 2>> map;
            SC_TEST_EXPECT(map.insertIfNotExists({"Ciao"_a8, "Fra"_a8}));
            SC_TEST_EXPECT(map.insertIfNotExists({"Bella"_a8, "Bro"_a8}));
            String* result1 = map.get("Ciao"_a8);
            SC_TEST_EXPECT(result1 and result1->view() == "Fra");
            auto result2 = map.get("Fail"_a8);
            SC_TEST_EXPECT(result2 == nullptr);
            SC_TEST_EXPECT(*map.get("Bella") == "Bro"_a8);
        }
        if (test_section("StrongID"))
        {
            struct Key
            {
                using ID = StrongID<Key>;
            };
            Map<Key::ID, String> map;

            const Key::ID key1 = Key::ID::generateUniqueKey(map);
            SC_TEST_EXPECT(map.insertIfNotExists({key1, "key1"_a8}));
            auto res = map.insertValueUniqueKey("key2"_a8);
            SC_TEST_EXPECT(res);
            const Key::ID key2 = *res;
            const Key::ID key3 = Key::ID::generateUniqueKey(map);
            SC_TEST_EXPECT(map.get(key1)->view() == "key1"_a8);
            SC_TEST_EXPECT(map.get(key2)->view() == "key2"_a8);
            SC_TEST_EXPECT(not map.get(key3));
        }
    }
};
