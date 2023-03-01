// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileSystemWalker.h"
namespace SC
{
struct FileSystemWalkerTest;
}

struct SC::FileSystemWalkerTest : public SC::TestCase
{
    FileSystemWalkerTest(SC::TestReport& report) : TestCase(report, "FileSystemWalkerTest")
    {
        using namespace SC;
        if (test_section("walk_recursive"))
        {
            FileSystemWalker walker;
            walker.options.recursive = false;
            SystemDirectories paths;
            SC_TEST_EXPECT(report.paths.init());
            SC_TEST_EXPECT(walker.init(report.paths.applicationRootDirectory.view()));
            while (walker.enumerateNext())
            {
                auto& item = walker.get();
                report.console.printLine(item.path); // This will take the UTF16 fast path on Windows
                if (item.isDirectory())
                {
                    SC_TEST_EXPECT(walker.recurseSubdirectory());
                }
            }
            SC_TEST_EXPECT(walker.checkErrors());
        }
    }
};
