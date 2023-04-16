// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "Array.h"
#include "Map.h"
#include "String.h"

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
            SC_TEST_EXPECT(map.insert({1, 2}));
            SC_TEST_EXPECT(map.insert({2, 3}));
            const int* value;
            SC_TEST_EXPECT(map.contains(1, &value) && *value == 2);
            SC_TEST_EXPECT(map.contains(2, &value) && *value == 3);
            SC_TEST_EXPECT(map.contains(2, &value) && *value == 3);
            size_t index = 0;
            SC_TEST_EXPECT(map.contains(2, &index) && index == 1);
            SC_TEST_EXPECT(not map.contains(3));
        }
        if (test_section("array"))
        {
            Map<String, String, Array<MapItem<String, String>, 2>> map;
            SC_TEST_EXPECT(map.insert({"Ciao"_a8, "Fra"_a8}));
            SC_TEST_EXPECT(map.insert({"Bella"_a8, "Bro"_a8}));
            SC_TEST_EXPECT(not map.insert({"Fail"_a8, "Fail"_a8}));
            const String* value;
            SC_TEST_EXPECT(map.contains("Ciao"_a8, &value) && *value == "Fra");
            SC_TEST_EXPECT(map.contains("Bella"_a8, &value) && *value == "Bro");
        }
        if (test_section("get"))
        {
            Map<String, String, Array<MapItem<String, String>, 2>> map;
            SC_TEST_EXPECT(map.insert({"Ciao"_a8, "Fra"_a8}));
            SC_TEST_EXPECT(map.insert({"Bella"_a8, "Bro"_a8}));
            auto result1 = map.get("Ciao"_a8);
            SC_TEST_EXPECT(*result1.releaseValue() == "Fra");
            auto result2 = map.get("Fail"_a8);
            SC_TEST_EXPECT(result2.isError());
            SC_TEST_EXPECT(getTestString(map, "Bella").releaseValue() == "Bro"_a8);
        }
    }

    template <typename T>
    static Result<const String&> getTestString(const T& map, StringView key)
    {
        SC_TRY(auto result, map.get(key));
        return *result;
    }
};
