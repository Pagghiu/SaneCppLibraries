// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Base/Limits.h"
#include "../Testing/Test.h"
#include "System.h"

namespace SC
{
struct SystemTest;
}

struct SC::SystemTest : public SC::TestCase
{
    SystemTest(SC::TestReport& report) : TestCase(report, "SystemTest")
    {
        using namespace SC;
        if (test_section("SystemDirectories"))
        {
            SystemDirectories directories;
            SC_TEST_EXPECT(directories.init());
            report.console.print("executableFile=\"{}\"\n", directories.executableFile.view());
            report.console.print("applicationRootDirectory=\"{}\"\n", directories.applicationRootDirectory.view());
        }
    }
};
