// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/ArenaMap.h"
#include "Libraries/Containers/Array.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Containers/VectorMap.h"
#include "Libraries/Containers/VectorSet.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct DebugVisualizersTest;
}
struct SC::DebugVisualizersTest : public SC::TestCase
{
    Buffer          buffer;
    SmallBuffer<16> smallBuffer;

    String stringUtf8  = "\xf0\x9f\x98\x82\xf0\x9f\x98\x8e UTF8 nice"_u8;
    String stringUtf16 = "\xE5\x65\x2C\x67\x9E\x8a\x9E\x8a\x9E\x8a\x00"_u16;

    SmallString<32> smallString = "SMALL STRING\xf0\x9f\x98\x82\xf0\x9f\x98\x8e";

    StringView twoFaces;
    StringSpan threeKanji;

    Vector<int>    ints    = {1, 2, 3, 4};
    Vector<double> doubles = {1.2, 2.3, 3.4};
    Vector<String> strings;
    Vector<String> stringsView;

    SmallVector<float, 3>  smallVector;
    VectorMap<String, int> map;
    VectorSet<StringView>  set;
    ArenaMap<String>       arenaMap; // This has no Debug visualizer

    Array<String, 3> array;
    DebugVisualizersTest(SC::TestReport& report) : TestCase(report, "DebugVisualizersTest")
    {
        using namespace SC;
        (void)buffer.append({"asdf"});
        smallBuffer = buffer;
        (void)buffer.append({" salver"});

        twoFaces   = stringUtf8.view().sliceStartLength(0, 2);
        threeKanji = stringUtf16.view().sliceStartLength(0, 3);
        (void)ints.resize(3);

        (void)strings.push_back(stringUtf8);
        (void)strings.push_back("SALVER");
        (void)strings.push_back(stringUtf16);

        (void)stringsView.push_back(twoFaces);
        (void)stringsView.push_back("SALVER");
        (void)stringsView.push_back(threeKanji);
        if (not report.quietMode)
        {
            report.console.printLine(twoFaces);
        }
        (void)smallVector.push_back(1.1f);
        (void)smallVector.push_back(2.2f);
        (void)smallVector.push_back(3.3f);
        (void)smallVector.push_back(4.4f);
        if (not report.quietMode)
        {
            report.console.print("{}\n", smallVector[0]);
        }

        (void)map.insertIfNotExists({"one", 1});
        (void)map.insertIfNotExists({"two", 2});
        (void)map.insertIfNotExists({"three", 3});

        (void)set.insert("3");
        (void)set.insert("3");
        (void)set.insert("3");
        (void)set.insert("2");
        (void)set.insert("1");

        (void)arenaMap.resize(10);
        (void)arenaMap.insert("one");
        auto k2 = arenaMap.insert("two");
        (void)arenaMap.insert("three");
        (void)arenaMap.remove(k2);
        array = {"Salve", "a", "Tutti"};
        (void)k2;
    }
};

namespace SC
{
void runDebugVisualizersTest(SC::TestReport& report) { DebugVisualizersTest test(report); }
} // namespace SC
