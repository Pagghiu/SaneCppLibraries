// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Limits.h"
#include "../Foundation/Test.h"
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

        if (test_section("SystemDebug::printBacktrace"))
        {
            SC_TEST_EXPECT(SystemDebug::printBacktrace());
            size_t frames = SystemDebug::printBacktrace(0, -1);
            SC_TEST_EXPECT(frames == 0);
        }
        if (test_section("SystemDebug::captureBacktrace"))
        {
            void*    traceBuffer[10];
            uint32_t hash   = 0;
            size_t   frames = SystemDebug::captureBacktrace(2, traceBuffer, sizeof(traceBuffer), &hash);
            SC_TEST_EXPECT(hash != 0);
            SC_TEST_EXPECT(frames != 0);
            constexpr auto maxVal = static_cast<size_t>(static_cast<int>(MaxValue())) + 1;
            frames                = SystemDebug::captureBacktrace(2, nullptr, maxVal * sizeof(void*), &hash);
            SC_TEST_EXPECT(frames == 0);
        }
        if (test_section("SystemDirectories"))
        {
            SystemDirectories directories;
            SC_TEST_EXPECT(directories.init());
            report.console.print("executableFile=\"{}\"\n", directories.executableFile.view());
            report.console.print("applicationRootDirectory=\"{}\"\n", directories.applicationRootDirectory.view());
        }
    }
};
