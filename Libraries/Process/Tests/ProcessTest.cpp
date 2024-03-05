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
            // Tries to launch a process that doesn't exist (and gets an error)
            Process process;
            SC_TEST_EXPECT(not process.launch({"DOCTORI", "ASDF"}));
        }
        if (test_section("Process inherit"))
        {
            // Launches a process that does exists
            Process process;
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(process.exec({"where", "where.exe"}));
#else
            SC_TEST_EXPECT(process.exec({"which", "sudo"}));
#endif
            // Will print either /usr/bin/sudo or C:\Windows\System32\where.exe to parent console
        }
        if (test_section("Process ignore"))
        {
            // Launches a process and ignore its stdoutput
            Process process;
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(process.exec({"where", "where.exe"}, Process::StdOut::Ignore()));
#else
            SC_TEST_EXPECT(process.exec({"which", "sudo"}, Process::StdOut::Ignore()));
#endif
            // Nothing will be printed on the parent stdout (console / file)
        }
        if (test_section("Process redirect output"))
        {
            // Launches a process and read its stdout into a String
            SmallString<255> output = StringEncoding::Ascii;
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(Process().exec({"where", "where.exe"}, output));
            SC_TEST_EXPECT(output.view() == "C:\\Windows\\System32\\where.exe\r\n");
#else
            SC_TEST_EXPECT(Process().exec({"which", "sudo"}, output));
            SC_TEST_EXPECT(output.view() == "/usr/bin/sudo\n");
#endif
        }
        if (test_section("ProcessChain inherit single"))
        {
            // Creates a process chain with a single process
            Process      p1;
            ProcessChain chain;
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(chain.pipe(p1, {"where", "where.exe"}));
#else
            SC_TEST_EXPECT(chain.pipe(p1, {"echo", "DOCTORI"}));
#endif
            SC_TEST_EXPECT(chain.exec());
        }
        if (test_section("ProcessChain inherit dual"))
        {
            // Executes two processes piping output of process p1 to input of process p2.
            // Then reads the output of the last process in the chain and check its correctness.
            ProcessChain chain;
            Process      p1, p2;
            // Print "Salve\nDoctori" on Windows and Posix and then grep for "Doc" (that returns "Doctori")
#if SC_PLATFORM_WINDOWS
            StringView expectedOutput = "Doctori\r\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"cmd", "/C", "echo", "Salve", "&", "echo", "Doctori"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"findstr", "Doc"}));
#else

            StringView expectedOutput = "Doctori\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"echo", "Salve\nDoctori"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"grep", "Doc"}));
#endif
            String output;
            SC_TEST_EXPECT(chain.exec(output));
            SC_TEST_EXPECT(output == expectedOutput);
        }
        if (test_section("ProcessChain pipe single"))
        {
            // Executes two processes piping output of process p1 to input of process p2.
            // Reads p2 stdout and stderr into a pair of Strings.
            ProcessChain chain;
            Process      p1;
#if SC_PLATFORM_WINDOWS
            StringView expectedOutput = "C:\\Windows\\System32\\where.exe\r\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"where", "where.exe"}));
#else
            StringView expectedOutput = "DOCTORI\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"echo", "DOCTORI"}));
#endif
            String stdOut(StringEncoding::Ascii);
            String stdErr(StringEncoding::Ascii);
            SC_TEST_EXPECT(chain.exec(stdOut, Process::StdIn::Inherit(), stdErr));
            SC_TEST_EXPECT(stdOut == expectedOutput);
            SC_TEST_EXPECT(stdErr.isEmpty());
        }
        if (test_section("ProcessChain pipe dual"))
        {
            // Chain two processes and read the stdout of the last one into a String using a PipeDescriptor
            ProcessChain chain;
            String       output(StringEncoding::Ascii);
            Process      p1, p2;
#if SC_PLATFORM_WINDOWS
            StringView expectedOutput = "WHERE [/R dir] [/Q] [/F] [/T] pattern...\r\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"where", "/?"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"findstr", "dir]"}));
#else
            StringView expectedOutput = "sbin\n";
            SC_TEST_EXPECT(chain.pipe(p1, {"ls", "/"}));
            SC_TEST_EXPECT(chain.pipe(p2, {"grep", "sbin"}));
#endif
            PipeDescriptor outputPipe;
            SC_TEST_EXPECT(chain.launch(outputPipe));
            SC_TEST_EXPECT(outputPipe.readPipe.readUntilEOF(output));
            SC_TEST_EXPECT(chain.waitForExitSync());
            SC_TEST_EXPECT(output == expectedOutput);
        }
    }

    SC::Result processSnippet1();
    SC::Result processSnippet2();
    SC::Result processSnippet3();
    SC::Result processSnippet4();
    SC::Result processSnippet5();
};

SC::Result SC::ProcessTest::processSnippet1()
{
    //! [ProcessSnippet1]
    // Example: execute child process (launch and wait for it to fully execute)
    SC_TRY(Process().exec({"cmd.exe", "-h"}));
    //! [ProcessSnippet1]
    return Result(true);
}

SC::Result SC::ProcessTest::processSnippet2()
{
    //! [ProcessSnippet2]
    // Example: execute child process, redirecting stdout to a string
    SmallString<256> output; // could be also just String
    SC_TRY(Process().exec({"where.exe", "winver"}, output));
    // Output now contains "C:\Windows\System32\winver.exe\n"
    //! [ProcessSnippet2]
    return Result(true);
}

SC::Result SC::ProcessTest::processSnippet3()
{
    //! [ProcessSnippet3]
    // Example: launch a child process and explicitly wait for it to finish execution
    Process process;
    SC_TRY(process.launch({"ls", "-l"}));
    // ...
    // Here you can do I/O to and from the spawned process
    // ...
    SC_TRY(process.waitForExitSync());
    // This is equivalent to process.exec({"ls", "-l")
    //! [ProcessSnippet3]
    return Result(true);
}

SC::Result SC::ProcessTest::processSnippet4()
{
    //! [ProcessSnippet4]
    // Example: execute child process, filling its stdin with a StringView
    // This is equivalent of shell command:
    // `echo "child process" | grep process`
    SC_TRY(Process().exec({"grep", "process"}, Process::StdOut::Inherit(), "child process"));
    //! [ProcessSnippet4]
    return Result(true);
}

SC::Result SC::ProcessTest::processSnippet5()
{
    //! [ProcessSnippet5]
    // Example: read process output using a pipe, using launch + waitForExitSync
    Process        process;
    PipeDescriptor outputPipe;
    SC_TRY(process.launch({"executable.exe", "--argument1", "--argument2"}, outputPipe));
    String output = StringEncoding::Ascii; // Could also use SmallString<N>
    SC_TRY(outputPipe.readPipe.readUntilEOF(output));
    SC_TRY(process.waitForExitSync());
    // ... Do something with the 'output' string
    //! [ProcessSnippet5]
    return Result(true);
}

namespace SC
{
void runProcessTest(SC::TestReport& report) { ProcessTest test(report); }
} // namespace SC
