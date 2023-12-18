// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../ArenaMap.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct ArenaMapTest;
}

struct SC::ArenaMapTest : public SC::TestCase
{
    ArenaMapTest(SC::TestReport& report) : TestCase(report, "ArenaMapTest")
    {
        using namespace SC;
        if (test_section("insert/get/remove/contains"))
        {
            ArenaMap<String> map;
            SC_TEST_EXPECT(not map.insert("ASD").isValid());
            SC_TEST_EXPECT(map.resize(2));
            SC_TEST_EXPECT(map.resize(3));
            ArenaMap<String>::Key keys[3];
            keys[0] = map.insert("ASD");
            SC_TEST_EXPECT(map.size() == 1);
            SC_TEST_EXPECT(not map.resize(4)); // cannot resize unless is empty
            keys[1]   = map.insert("DSA");
            keys[2]   = map.insert("BDA");
            int index = 0;
            for (const auto& it : map)
            {
                switch (index++)
                {
                case 0: SC_TEST_EXPECT(it == "ASD"); break;
                case 1: SC_TEST_EXPECT(it == "DSA"); break;
                case 2: SC_TEST_EXPECT(it == "BDA"); break;
                }
            }
            SC_TEST_EXPECT(map.size() == 3);
            SC_TEST_EXPECT(not map.insert("123").isValid());

            SC_TEST_EXPECT(map.get(keys[0])->view() == "ASD");
            SC_TEST_EXPECT(map.get(keys[1])->view() == "DSA");
            SC_TEST_EXPECT(map.get(keys[2])->view() == "BDA");
            ArenaMap<String>::Key key;
            SC_TEST_EXPECT(map.containsValue("BDA", &key) and key.isValid());
            SC_TEST_EXPECT(not map.containsValue("__ASD__"));
            SC_TEST_EXPECT(map.containsKey(keys[1]));
            SC_TEST_EXPECT(map.remove(keys[1]));
            SC_TEST_EXPECT(not map.remove(keys[1]));
            SC_TEST_EXPECT(not map.containsKey(keys[1]));
            SC_TEST_EXPECT(map.get(keys[1]) == nullptr);

            index = 0;
            for (auto& it : map)
            {
                switch (index++)
                {
                case 0: SC_TEST_EXPECT(it == "ASD"); break;
                case 1: SC_TEST_EXPECT(it == "BDA"); break;
                }
            }

            const auto newKey = map.insert("123");
            SC_TEST_EXPECT(map.containsKey(newKey));
            SC_TEST_EXPECT(map.get(newKey)->view() == "123");
            index = 0;
            for (auto& it : map)
            {
                switch (index++)
                {
                case 0: SC_TEST_EXPECT(it == "ASD"); break;
                case 1:
                    SC_TEST_EXPECT(it == "123");
                    it = "456";
                    break;
                case 2: SC_TEST_EXPECT(it == "BDA"); break;
                }
            }
            SC_TEST_EXPECT(map.get(newKey)->view() == "456");
            SC_TEST_EXPECT(not map.containsKey(keys[1]));
            SC_TEST_EXPECT(map.get(keys[1]) == nullptr);
        }
        if (test_section("copy"))
        {
            ArenaMap<String> map, mapCopy, mapMove;
            SC_TEST_EXPECT(map.resize(3));
            ArenaMap<String>::Key keys[3];
            keys[0] = map.insert("ASD");
            keys[1] = map.insert("DSA");
            keys[2] = map.insert("BDA");
            mapCopy = map;
            mapMove = move(map);

#ifndef __clang_analyzer__
            SC_TEST_EXPECT(map.size() == 0);
#endif // not __clang_analyzer__
            SC_TEST_EXPECT(mapCopy.size() == 3);
            SC_TEST_EXPECT(mapMove.size() == 3);

            SC_TEST_EXPECT(mapCopy.get(keys[0])->view() == "ASD");
            SC_TEST_EXPECT(mapCopy.get(keys[1])->view() == "DSA");
            SC_TEST_EXPECT(mapCopy.get(keys[2])->view() == "BDA");

            SC_TEST_EXPECT(mapCopy.remove(keys[0]));
            SC_TEST_EXPECT(mapCopy.size() == 2);

            SC_TEST_EXPECT(mapMove.get(keys[0])->view() == "ASD");
            SC_TEST_EXPECT(mapMove.get(keys[1])->view() == "DSA");
            SC_TEST_EXPECT(mapMove.get(keys[2])->view() == "BDA");
        }
    }
};

namespace SC
{
void runArenaMapTest(SC::TestReport& report) { ArenaMapTest test(report); }
} // namespace SC
