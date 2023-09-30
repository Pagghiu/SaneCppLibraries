// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Libraries/Foundation/Base/Memory.h"
#include "../../Libraries/Foundation/Containers/ArenaMap.h"
#include "../../Libraries/Foundation/Containers/SmallVector.h"
#include "../../Libraries/Foundation/Containers/VectorMap.h"
#include "../../Libraries/Foundation/Containers/VectorSet.h"
#include "../../Libraries/Foundation/Strings/String.h"
#include "../../Libraries/Testing/Test.h"

namespace SC
{
struct DebugVisualizersTest;
}
struct SC::DebugVisualizersTest : public SC::TestCase
{
    DebugVisualizersTest(SC::TestReport& report) : TestCase(report, "DebugVisualizersTest")
    {
        using namespace SC;

        String     stringutf8  = StringView("\xf0\x9f\x98\x82\xf0\x9f\x98\x8e UTF8 yees"_u8);
        String     stringutf16 = StringView("\xE5\x65\x2C\x67\x9E\x8a\x9E\x8a\x9E\x8a\x00"_u16);
        StringView twoFaces    = stringutf8.view().sliceStartLengthBytes(0, 8);
        StringView threeKanji  = stringutf16.view().sliceStartLengthBytes(0, 6);
        (void)twoFaces;
        (void)threeKanji;
        Vector<int> vints = {1, 2, 3, 4};
        (void)vints.resize(3);
        Vector<double> vdoubles = {1.2, 2.3, 3.4};

        Vector<String> vstrings;
        (void)vstrings.push_back(stringutf8);
        (void)vstrings.push_back("SALVER"_a8);
        (void)vstrings.push_back(stringutf16);
        Vector<String> vstringsView;
        (void)vstringsView.push_back(twoFaces);
        (void)vstringsView.push_back("SALVER"_a8);
        (void)vstringsView.push_back(threeKanji);
        report.console.printLine(twoFaces);
        SmallVector<float, 3> smallVector;
        (void)smallVector.push_back(1.1f);
        (void)smallVector.push_back(2.2f);
        (void)smallVector.push_back(3.3f);
        (void)smallVector.push_back(4.4f);
        report.console.print("{}\n", smallVector[0]);
        SmallString<10> ss = "asd"_a8;

        VectorMap<String, int> map;
        (void)map.insertIfNotExists({"one"_a8, 1});
        (void)map.insertIfNotExists({"two"_a8, 2});
        (void)map.insertIfNotExists({"three"_a8, 3});

        VectorSet<StringView> set;
        (void)set.insert("3");
        (void)set.insert("3");
        (void)set.insert("3");
        (void)set.insert("2");
        (void)set.insert("1");

        ArenaMap<String> arenaMap;
        (void)arenaMap.resize(10);
        (void)arenaMap.insert("one"_a8);
        auto k2 = arenaMap.insert("two"_a8);
        (void)arenaMap.insert("three"_a8);
        (void)arenaMap.remove(k2);
        (void)k2;
    }
};
