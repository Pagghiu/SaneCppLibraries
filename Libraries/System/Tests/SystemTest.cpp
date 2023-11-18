// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../System.h"
#include "../../Foundation/Limits.h"
#include "../../Testing/Testing.h"

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
            report.console.print("executableFile=\"{}\"\n", directories.getExecutablePath());
            report.console.print("applicationRootDirectory=\"{}\"\n", directories.getApplicationPath());
        }
    }
};

namespace SC
{
void runSystemTest(SC::TestReport& report) { SystemTest test(report); }
} // namespace SC
