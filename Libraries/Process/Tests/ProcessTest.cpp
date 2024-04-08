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
            processError();
        }
        if (test_section("Process inherit"))
        {
            processInheritStdout();
        }
        if (test_section("Process ignore"))
        {
            processIgnoreStdout();
        }
        if (test_section("Process redirect output"))
        {
            processRedirectStdout();
        }
        if (test_section("ProcessChain inherit single"))
        {
            processChainInheritSingle();
        }
        if (test_section("ProcessChain inherit dual"))
        {
            processChainInheritDual();
        }
        if (test_section("ProcessChain pipe single"))
        {
            processChainPipeSingle();
        }
        if (test_section("ProcessChain pipe dual"))
        {
            processChainPipeDual();
        }

        // This section is not executed as a test, but explicitly executed in a child process by some tests below
        if (test_section("ProcessEnvironment", Execute::OnlyExplicit))
        {
            processEnvironmentPrint();
        }
        if (test_section("Process environment new environment var"))
        {
            processEnvironmentNewVar();
        }
        if (test_section("Process environment re-define parent environment var"))
        {
            processEnvironmentRedefineParentVar();
        }
        if (test_section("Process environment disable parent environment var"))
        {
            processEnvironmentDisableInheritance();
        }
    }

    void processError();
    void processInheritStdout();
    void processIgnoreStdout();
    void processRedirectStdout();
    void processChainInheritSingle();
    void processChainInheritDual();
    void processChainPipeSingle();
    void processChainPipeDual();
    void processEnvironmentPrint();
    void processEnvironmentNewVar();
    void processEnvironmentRedefineParentVar();
    void processEnvironmentDisableInheritance();

    Result spawnChildAndPrintEnvironmentVars(Process& process, String& output);

    Result processSnippet1();
    Result processSnippet2();
    Result processSnippet3();
    Result processSnippet4();
    Result processSnippet5();
};

void SC::ProcessTest::processError()
{
    // Tries to launch a process that doesn't exist (and gets an error)
    Process process;
    SC_TEST_EXPECT(not process.launch({"DOCTORI", "ASDF"}));
}

void SC::ProcessTest::processInheritStdout()
{
    // Launches a process that does exists, inheriting its standard output
    switch (HostPlatform)
    {
    case Platform::Windows: {
        SC_TEST_EXPECT(Process().exec({"where", "where.exe"}));
    }
    break;
    default: { // Posix
        SC_TEST_EXPECT(Process().exec({"which", "sudo"}));
    }
    break;
    }

    // Will print either /usr/bin/sudo or C:\Windows\System32\where.exe to parent console
}

void SC::ProcessTest::processIgnoreStdout()
{
    // Launches a process ignoring its standard output
    switch (HostPlatform)
    {
    case Platform::Windows: {
        SC_TEST_EXPECT(Process().exec({"where", "where.exe"}, Process::StdOut::Ignore()));
    }
    break;
    default: { // Posix
        SC_TEST_EXPECT(Process().exec({"which", "sudo"}, Process::StdOut::Ignore()));
    }
    break;
    }
    // Nothing will be printed on the parent stdout (console / file)
}

void SC::ProcessTest::processRedirectStdout()
{
    // Launches a process and read its stdout into a String
    SmallString<255> output = StringEncoding::Ascii;
    switch (HostPlatform)
    {
    case Platform::Windows: {
        SC_TEST_EXPECT(Process().exec({"where", "where.exe"}, output));
        SC_TEST_EXPECT(output.view() == "C:\\Windows\\System32\\where.exe\r\n");
    }
    break;
    default: { // Posix
        SC_TEST_EXPECT(Process().exec({"which", "sudo"}, output));
        SC_TEST_EXPECT(output.view() == "/usr/bin/sudo\n");
    }
    break;
    }
}

void SC::ProcessTest::processChainInheritSingle()
{
    //! [processChainInheritSingleSnippet]
    // Creates a process chain with a single process
    Process      p1;
    ProcessChain chain;
    switch (HostPlatform)
    {
    case Platform::Windows: {
        SC_TEST_EXPECT(chain.pipe(p1, {"where", "where.exe"}));
    }
    break;
    default: { // Posix
        SC_TEST_EXPECT(chain.pipe(p1, {"echo", "DOCTORI"}));
    }
    break;
    }
    SC_TEST_EXPECT(chain.exec());
    //! [processChainInheritSingleSnippet]
}

void SC::ProcessTest::processChainInheritDual()
{
    //! [processChainInheritDualSnippet]
    // Executes two processes piping output of process p1 to input of process p2.
    // Then reads the output of the last process in the chain and check its correctness.
    ProcessChain chain;
    Process      p1, p2;
    // Print "Salve\nDoctori" on Windows and Posix and then grep for "Doc"
    StringView expectedOutput;
    switch (HostPlatform)
    {
    case Platform::Windows: {
        expectedOutput = "Doctori\r\n";
        SC_TEST_EXPECT(chain.pipe(p1, {"cmd", "/C", "echo", "Salve", "&", "echo", "Doctori"}));
        SC_TEST_EXPECT(chain.pipe(p2, {"findstr", "Doc"}));
    }
    break;
    default: { // Posix
        expectedOutput = "Doctori\n";
        SC_TEST_EXPECT(chain.pipe(p1, {"echo", "Salve\nDoctori"}));
        SC_TEST_EXPECT(chain.pipe(p2, {"grep", "Doc"}));
    }
    break;
    }
    String output;
    SC_TEST_EXPECT(chain.exec(output));
    SC_TEST_EXPECT(output == expectedOutput);
    //! [processChainInheritDualSnippet]
}

void SC::ProcessTest::processChainPipeSingle()
{
    //! [processChainPipeSingleSnippet]
    // Executes two processes piping output of process p1 to input of process p2.
    // Reads p2 stdout and stderr into a pair of Strings.
    ProcessChain chain;
    Process      p1;
    StringView   expectedOutput;
    switch (HostPlatform)
    {
    case Platform::Windows: {
        expectedOutput = "C:\\Windows\\System32\\where.exe\r\n";
        SC_TEST_EXPECT(chain.pipe(p1, {"where", "where.exe"}));
    }
    break;
    default: { // Posix
        expectedOutput = "DOCTORI\n";
        SC_TEST_EXPECT(chain.pipe(p1, {"echo", "DOCTORI"}));
    }
    break;
    }

    String stdOut(StringEncoding::Ascii);
    String stdErr(StringEncoding::Ascii);
    SC_TEST_EXPECT(chain.exec(stdOut, Process::StdIn::Inherit(), stdErr));
    SC_TEST_EXPECT(stdOut == expectedOutput);
    SC_TEST_EXPECT(stdErr.isEmpty());
    //! [processChainPipeSingleSnippet]
}

void SC::ProcessTest::processChainPipeDual()
{
    //! [processChainPipeDualSnippet]
    // Chain two processes and read the last stdout into a String (using a pipe)
    ProcessChain chain;

    String  output(StringEncoding::Ascii);
    Process p1, p2;

    StringView expectedOutput;
    switch (HostPlatform)
    {
    case Platform::Windows: {
        expectedOutput = "WHERE [/R dir] [/Q] [/F] [/T] pattern...\r\n";
        SC_TEST_EXPECT(chain.pipe(p1, {"where", "/?"}));
        SC_TEST_EXPECT(chain.pipe(p2, {"findstr", "dir]"}));
    }
    break;
    default: { // Posix
        expectedOutput = "sbin\n";
        SC_TEST_EXPECT(chain.pipe(p1, {"ls", "/"}));
        SC_TEST_EXPECT(chain.pipe(p2, {"grep", "sbin"}));
    }
    break;
    }
    PipeDescriptor outputPipe;
    SC_TEST_EXPECT(chain.launch(outputPipe));
    SC_TEST_EXPECT(outputPipe.readPipe.readUntilEOF(output));
    SC_TEST_EXPECT(chain.waitForExitSync());
    SC_TEST_EXPECT(output == expectedOutput);
    //! [processChainPipeDualSnippet]
}

// This section is not executed as a test, but explicitly executed in a child process by some tests below
void SC::ProcessTest::processEnvironmentPrint()
{
    //! [ProcessEnvironmentPrint]
    SC::ProcessEnvironment environment;
    for (size_t idx = 0; idx < environment.size(); ++idx)
    {
        StringView name, value;
        (void)environment.get(idx, name, value);
        if (value.isEmpty())
        {
            report.console.printLine(name);
        }
        else
        {
            report.console.print(name);
            report.console.print("=");
            report.console.printLine(value);
        }
    }
    //! [ProcessEnvironmentPrint]
}

SC::Result SC::ProcessTest::spawnChildAndPrintEnvironmentVars(Process& process, String& output)
{
    // This calls the above ProcessTest::processEnvironmentPrint() in a child process
    return process.exec(
        {report.executableFile, "--quiet", "--test", "ProcessTest", "--test-section", "ProcessEnvironment"}, output);
}

void SC::ProcessTest::processEnvironmentNewVar()
{
    //! [ProcessEnvironmentNewVar]
    Process process;
    // This child process will inherit parent environment variables plus NewEnvVar
    SC_TEST_EXPECT(process.setEnvironment("NewEnvVar", "SomeValue"));
    String output;
    // Spawn the child process writing all env variables as KEY=VALUE\n to stdout, redirected to output
    SC_TEST_EXPECT(spawnChildAndPrintEnvironmentVars(process, output));
    // We can check that the NewEnvVar has been set to SomeValue
    SC_TEST_EXPECT(output.view().containsString("NewEnvVar=SomeValue"));
    // PATH env var exists because we are inheriting environment
    SC_TEST_EXPECT(output.view().containsString("PATH="));
    //! [ProcessEnvironmentNewVar]
}

void SC::ProcessTest::processEnvironmentRedefineParentVar()
{
    //! [ProcessEnvironmentRedefine]
    Process process;
    // This child process will inherit parent environment variables but we re-define PATH
    SC_TEST_EXPECT(process.setEnvironment("PATH", "/usr/sane_cpp_binaries"));
    String output;
    // Spawn the child process writing all env variables as KEY=VALUE\n to stdout, redirected to output
    SC_TEST_EXPECT(spawnChildAndPrintEnvironmentVars(process, output));
    // PATH env var has been re-defined
    SC_TEST_EXPECT(output.view().containsString("PATH=/usr/sane_cpp_binaries"));
    //! [ProcessEnvironmentRedefine]
}

void SC::ProcessTest::processEnvironmentDisableInheritance()
{
    //! [ProcessEnvironmentDisableInheritance]
    Process process;
    process.inheritParentEnvironmentVariables(false);
    String output;
    // Spawn the child process writing all env variables as KEY=VALUE\n to stdout, redirected to output
    SC_TEST_EXPECT(spawnChildAndPrintEnvironmentVars(process, output));
    // PATH env var doesn't exist because of Proces::inheritParentEnvironmentVariables(false)
    SC_TEST_EXPECT(not output.view().containsString("PATH="));
    //! [ProcessEnvironmentDisableInheritance]
}

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
    // This is equivalent to process.exec({"ls", "-l"})
    //! [ProcessSnippet3]
    return Result(true);
}

SC::Result SC::ProcessTest::processSnippet4()
{
    //! [ProcessSnippet4]
    // Example: execute child process, filling its stdin with a StringView
    // This is equivalent of shell command:
    // `echo "child process" | grep process`
    SC_TRY(Process().exec({"grep", "process"}, Process::StdOut::Inherit(), "child proc"));
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
