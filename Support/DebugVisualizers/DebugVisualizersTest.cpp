// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Libraries/Containers/ArenaMap.h"
#include "../../Libraries/Containers/SmallVector.h"
#include "../../Libraries/Containers/VectorMap.h"
#include "../../Libraries/Containers/VectorSet.h"
#include "../../Libraries/Foundation/Memory.h"
#include "../../Libraries/Strings/SmallString.h"
#include "../../Libraries/Testing/Testing.h"

namespace SC
{
struct DebugVisualizersTest;
}
struct SC::DebugVisualizersTest : public SC::TestCase
{
    DebugVisualizersTest(SC::TestReport& report) : TestCase(report, "DebugVisualizersTest")
    {
        using namespace SC;

        String     stringUtf8  = StringView("\xf0\x9f\x98\x82\xf0\x9f\x98\x8e UTF8 nice"_u8);
        String     stringUtf16 = StringView("\xE5\x65\x2C\x67\x9E\x8a\x9E\x8a\x9E\x8a\x00"_u16);
        StringView twoFaces    = stringUtf8.view().sliceStartLengthBytes(0, 8);
        StringView threeKanji  = stringUtf16.view().sliceStartLengthBytes(0, 6);
        (void)twoFaces;
        (void)threeKanji;
        Vector<int> ints = {1, 2, 3, 4};
        (void)ints.resize(3);
        Vector<double> doubles = {1.2, 2.3, 3.4};

        Vector<String> strings;
        (void)strings.push_back(stringUtf8);
        (void)strings.push_back("SALVER");
        (void)strings.push_back(stringUtf16);
        Vector<String> stringsView;
        (void)stringsView.push_back(twoFaces);
        (void)stringsView.push_back("SALVER");
        (void)stringsView.push_back(threeKanji);
        report.console.printLine(twoFaces);
        SmallVector<float, 3> smallVector;
        (void)smallVector.push_back(1.1f);
        (void)smallVector.push_back(2.2f);
        (void)smallVector.push_back(3.3f);
        (void)smallVector.push_back(4.4f);
        report.console.print("{}\n", smallVector[0]);
        SmallString<10> ss = "asd";

        VectorMap<String, int> map;
        (void)map.insertIfNotExists({"one", 1});
        (void)map.insertIfNotExists({"two", 2});
        (void)map.insertIfNotExists({"three", 3});

        VectorSet<StringView> set;
        (void)set.insert("3");
        (void)set.insert("3");
        (void)set.insert("3");
        (void)set.insert("2");
        (void)set.insert("1");

        ArenaMap<String> arenaMap;
        (void)arenaMap.resize(10);
        (void)arenaMap.insert("one");
        auto k2 = arenaMap.insert("two");
        (void)arenaMap.insert("three");
        (void)arenaMap.remove(k2);
        (void)k2;
    }
};

namespace SC
{
void runDebugVisualizersTest(SC::TestReport& report) { DebugVisualizersTest test(report); }
} // namespace SC
