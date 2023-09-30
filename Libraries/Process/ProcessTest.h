// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Async/EventLoop.h"
#include "../Testing/Test.h"
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
            Process process;
#if SC_PLATFORM_APPLE
            StringView expectedOutput = "/usr/bin/sudo\n"_a8;
            SC_TEST_EXPECT(process.formatCommand("which", "sudo"));
#else
            StringView expectedOutput = "C:\\Windows\\System32\\where.exe\r\n"_a8;
            SC_TEST_EXPECT(process.formatCommand("where", "where.exe"));
#endif
            PipeDescriptor outputPipe;
            SC_TEST_EXPECT(process.redirectStdOutTo(outputPipe));
            SC_TEST_EXPECT(process.launch());

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
            String       output(StringEncoding::Ascii);
            Process      p1;
#if SC_PLATFORM_APPLE
            StringView expectedOutput = "asd\n"_a8;
            SC_TEST_EXPECT(chain.pipe(p1, "echo", "asd"));
#else
            StringView expectedOutput = "C:\\Windows\\System32\\where.exe\r\n"_a8;
            SC_TEST_EXPECT(chain.pipe(p1, "where", "where.exe"));
#endif
            ProcessChainOptions options;
            options.pipeSTDOUT = true;
            SC_TEST_EXPECT(chain.launch(options));
            SC_TEST_EXPECT(chain.readStdOutUntilEOFSync(output));
            SC_TEST_EXPECT(chain.waitForExitSync());
            SC_TEST_EXPECT(output == expectedOutput);
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
            StringView expectedOutput = "Desktop\n"_a8;
            SC_TEST_EXPECT(chain.pipe(p1, {"ls", "~"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"grep", "Desktop"}));
#else
            StringView expectedOutput = "WHERE [/R dir] [/Q] [/F] [/T] pattern...\r\n"_a8;
            SC_TEST_EXPECT(chain.pipe(p1, {"where", "/?"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"findstr", "dir]"}));
#endif
            ProcessChainOptions options;
            options.pipeSTDOUT = true;
            SC_TEST_EXPECT(chain.launch(options));
            SC_TEST_EXPECT(chain.readStdOutUntilEOFSync(output));
            SC_TEST_EXPECT(chain.waitForExitSync());
            SC_TEST_EXPECT(output == expectedOutput);
            SC_TEST_EXPECT(not hasError);
        }
        if (test_section("Process EventLoop"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Process process;
#if SC_PLATFORM_APPLE
            SC_TEST_EXPECT(process.launch("which", "sudo"));
#else
            SC_TEST_EXPECT(process.launch("where", "where.exe"));
#endif
            ProcessDescriptor::Handle processHandle;
            SC_TEST_EXPECT(process.handle.get(processHandle, ReturnCode::Error("Invalid Handle")));
            ProcessDescriptor::ExitStatus exitStatus;
            AsyncProcessExit              async;
            async.callback = [&](AsyncProcessExit::Result& res) { SC_TEST_EXPECT(res.moveTo(exitStatus)); };
            SC_TEST_EXPECT(async.start(eventLoop, processHandle));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(exitStatus.status.hasValue());
        }
    }
};
