// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Process.h"
#include "../../Async/Async.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct ProcessTest;
}

struct SC::ProcessTest : public SC::TestCase
{
    ProcessTest(SC::TestReport& report) : TestCase(report, "ProcessTest")
    {
        using namespace SC;
        if (test_section("Process error"))
        {
            Process process;
            SC_TEST_EXPECT(process.launch("piadsfj", "afsdkj"));
            SC_TEST_EXPECT(not process.waitForExitSync());
        }
        if (test_section("Process inherit"))
        {
            Process process;
#if SC_PLATFORM_APPLE
            SC_TEST_EXPECT(process.launch("which", "sudo"));
#else
            SC_TEST_EXPECT(process.launch("where", "where.exe"));
#endif
            SC_TEST_EXPECT(process.waitForExitSync());
        }
        if (test_section("Process piped"))
        {
            Process        process;
            PipeDescriptor outputPipe;
            SC_TEST_EXPECT(process.redirectStdOutTo(outputPipe));
#if SC_PLATFORM_APPLE
            StringView expectedOutput = "/usr/bin/sudo\n";
            SC_TEST_EXPECT(process.launch("which", "sudo"));
#else
            StringView expectedOutput = "C:\\Windows\\System32\\where.exe\r\n";
            SC_TEST_EXPECT(process.launch("where", "where.exe"));
#endif
            SmallString<255> output = StringEncoding::Ascii;
            SC_TEST_EXPECT(outputPipe.readPipe.readUntilEOF(output));
            SC_TEST_EXPECT(process.waitForExitSync());
            SC_TEST_EXPECT(output.view() == expectedOutput);
        }
        if (test_section("ProcessChain inherit single"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessChain::Error&) { hasError = true; };
            Process      p1;
            ProcessChain chain(onErr);
#if SC_PLATFORM_APPLE
            SC_TEST_EXPECT(chain.pipe(p1, {"ls", "~/Public"}));
#else
            SC_TEST_EXPECT(chain.pipe(p1, {"where", "where.exe"}));
#endif
            SC_TEST_EXPECT(chain.launch());
            SC_TEST_EXPECT(chain.waitForExitSync());
            SC_TEST_EXPECT(not hasError);
        }
        if (test_section("ProcessChain inherit dual"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessChain::Error&) { hasError = true; };
            ProcessChain chain(onErr);
            Process      p1, p2;
#if SC_PLATFORM_APPLE
            SC_TEST_EXPECT(chain.pipe(p1, "ls", "~"));
            SC_TEST_EXPECT(chain.pipe(p2, "grep", "Desktop"));
#else
            SC_TEST_EXPECT(chain.pipe(p1, "where", "/?"));
            SC_TEST_EXPECT(chain.pipe(p2, "findstr", "dir]"));
#endif
            SC_TEST_EXPECT(chain.launch());
            SC_TEST_EXPECT(chain.waitForExitSync());
            SC_TEST_EXPECT(not hasError);
        }
        if (test_section("ProcessChain pipe single"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessChain::Error&) { hasError = true; };
            ProcessChain chain(onErr);
            Process      p1;
#if SC_PLATFORM_APPLE
            StringView expectedOutput = "a s d\n";
            SC_TEST_EXPECT(chain.pipe(p1, "echo", "a s d"));
#else
            StringView expectedOutput = "C:\\Windows\\System32\\where.exe\r\n";
            SC_TEST_EXPECT(chain.pipe(p1, "where", "where.exe"));
#endif
            ProcessChain::Options options;
            options.pipeSTDOUT = true;
            options.pipeSTDERR = true;
            SC_TEST_EXPECT(chain.launch(options));
            String stdOut(StringEncoding::Ascii);
            String stdErr(StringEncoding::Ascii);
            SC_TEST_EXPECT(chain.readStdOutUntilEOFSync(stdOut));
            SC_TEST_EXPECT(chain.readStdErrUntilEOFSync(stdErr));
            SC_TEST_EXPECT(chain.waitForExitSync());
            SC_TEST_EXPECT(stdOut == expectedOutput);
            SC_TEST_EXPECT(stdErr.isEmpty());
            SC_TEST_EXPECT(not hasError);
        }
        if (test_section("ProcessChain pipe dual"))
        {
            bool         hasError = false;
            auto         onErr    = [&](const ProcessChain::Error&) { hasError = true; };
            ProcessChain chain(onErr);
            String       output(StringEncoding::Ascii);
            Process      p1, p2;
#if SC_PLATFORM_APPLE
            StringView expectedOutput = "Desktop\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"ls", "~"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"grep", "Desktop"}));
#else
            StringView expectedOutput = "WHERE [/R dir] [/Q] [/F] [/T] pattern...\r\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"where", "/?"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"findstr", "dir]"}));
#endif
            ProcessChain::Options options;
            options.pipeSTDOUT = true;
            SC_TEST_EXPECT(chain.launch(options));
            SC_TEST_EXPECT(chain.readStdOutUntilEOFSync(output));
            SC_TEST_EXPECT(chain.waitForExitSync());
            SC_TEST_EXPECT(output == expectedOutput);
            SC_TEST_EXPECT(not hasError);
        }
    }
};

namespace SC
{
void runProcessTest(SC::TestReport& report) { ProcessTest test(report); }
} // namespace SC
