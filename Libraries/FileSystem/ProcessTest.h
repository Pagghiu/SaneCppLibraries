// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Test.h"
#include "Process.h"

namespace SC
{
struct ProcessTest;
}

struct SC::ProcessTest : public SC::TestCase
{
    ProcessTest(SC::TestReport& report) : TestCase(report, "ProcessTest")
    {
        using namespace SC;
        // Additional USAGES HYPOTESIS
        // using cmd     = Span<const StringView>;
        // SC_TRY_IF((shell | cmd{"ls", "-l"} | cmd{"grep", "salver"}));
        if (test_section("inherit single"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessShell::Error& err) { hasError = true; };
            ProcessShell shell(onErr);
#if SC_PLATFORM_APPLE
            SC_TEST_EXPECT(shell.pipe("ls", "~/Public").launch());
#else

            SC_TEST_EXPECT(shell.pipe("where", "where.exe").launch());
#endif
            SC_TEST_EXPECT(shell.waitSync());
            SC_TEST_EXPECT(not hasError);
        }
        if (test_section("inherit piped"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessShell::Error& err) { hasError = true; };
            ProcessShell shell(onErr);
#if SC_PLATFORM_APPLE
            SC_TEST_EXPECT(shell.pipe("ls", "~").pipe("grep", "Desktop").launch());
#else
            SC_TEST_EXPECT(shell.pipe("where", "/?").pipe("findstr", "dir]").launch());
#endif
            SC_TEST_EXPECT(shell.waitSync());
            SC_TEST_EXPECT(not hasError);
        }
        if (test_section("pipe single"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessShell::Error& err) { hasError = true; };
            ProcessShell shell(onErr);
            shell.options.pipeSTDOUT = true;
            String output(StringEncoding::Ascii);
#if SC_PLATFORM_APPLE
            StringView expectedOutput = "asd\n"_a8;
            SC_TEST_EXPECT(shell.pipe("echo", "asd").launch());
#else
            StringView expectedOutput = "C:\\Windows\\System32\\where.exe\r\n"_a8;
            SC_TEST_EXPECT(shell.pipe("where", "where.exe").launch());
#endif
            SC_TEST_EXPECT(shell.readOutputSync(&output));
            SC_TEST_EXPECT(shell.waitSync());
            SC_TEST_EXPECT(output == expectedOutput);
            SC_TEST_EXPECT(not hasError);
        }
        if (test_section("pipe dual"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessShell::Error& err) { hasError = true; };
            ProcessShell shell(onErr);
            shell.options.pipeSTDOUT = true;
            String output(StringEncoding::Ascii);
#if SC_PLATFORM_APPLE
            StringView expectedOutput = "Desktop\n"_a8;
            SC_TEST_EXPECT(shell.pipe("ls", "~").pipe("grep", "Desktop").launch());
#else
            StringView expectedOutput = "WHERE [/R dir] [/Q] [/F] [/T] pattern...\r\n"_a8;
            SC_TEST_EXPECT(shell.pipe("where", "/?").pipe("findstr", "dir]").launch());
#endif
            SC_TEST_EXPECT(shell.readOutputSync(&output));
            SC_TEST_EXPECT(shell.waitSync());
            SC_TEST_EXPECT(output == expectedOutput);
            SC_TEST_EXPECT(not hasError);
        }
    }
};
