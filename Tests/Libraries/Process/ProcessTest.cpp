// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Process/Process.h"
#include "Libraries/Async/Async.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct ProcessTest;
}

struct SC::ProcessTest : public SC::TestCase
{
    Vector<native_char_t> commandArena;
    Vector<native_char_t> environmentArena;
    ProcessTest(SC::TestReport& report) : TestCase(report, "ProcessTest")
    {
        // There are some crazy large environment variables on github CI runners...
        SC_ASSERT_RELEASE(commandArena.resize(16 * 1024));
        SC_ASSERT_RELEASE(environmentArena.resize(64 * 1024));
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

        // Process fork doesn't work under windows ARM64 <-> x86_64 emulation
        if (not Process::isWindowsEmulatedProcess())
        {
            if (test_section("Process fork"))
            {
                processFork();
            }
        }
#if SC_XCTEST
#else
        // These tests cannot be run when tests are compiled to a dylib under XCTest
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
#endif
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
    void processFork();

    Result spawnChildAndPrintEnvironmentVars(Process& process, String& output);

    Result quickSheet();
    Result processSnippet1();
    Result processSnippet2();
    Result processSnippet3();
    Result processSnippet4();
    Result processSnippet5();
};

void SC::ProcessTest::processError()
{
    // Tries to launch a process that doesn't exist (and gets an error)
    Process process(commandArena.toSpan(), environmentArena.toSpan());
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
        SC_TEST_EXPECT(Process().exec({"which", "su"}));
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
        SC_TEST_EXPECT(Process().exec({"which", "su"}, Process::StdOut::Ignore()));
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
        SC_TEST_EXPECT(Process().exec({"which", "su"}, output));
        SC_TEST_EXPECT(output.view() == "/bin/su\n" or output.view() == "/usr/bin/su\n");
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
    PipeOptions pipeOptions;
    pipeOptions.writeInheritable = true; // This is correct but not strictly necessary...
    PipeDescriptor outputPipe;
    SC_TEST_EXPECT(outputPipe.createPipe(pipeOptions));
    SC_TEST_EXPECT(chain.launch(outputPipe));
    SC_TEST_EXPECT(outputPipe.readPipe.readUntilEOF(output));
    SC_TEST_EXPECT(chain.waitForExitSync());
    SC_TEST_EXPECT(StringView(output.view()).startsWith(expectedOutput));
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
    Process process(commandArena.toSpan(), environmentArena.toSpan());
    // This child process will inherit parent environment variables plus NewEnvVar
    SC_TEST_EXPECT(process.setEnvironment("NewEnvVar", "SomeValue"));
    String output;
    // Spawn the child process writing all env variables as KEY=VALUE\n to stdout, redirected to output
    SC_TEST_EXPECT(spawnChildAndPrintEnvironmentVars(process, output));
    // We can check that the NewEnvVar has been set to SomeValue
    StringView out = output.view();
    SC_TEST_EXPECT(out.containsString("NewEnvVar=SomeValue"));
    // PATH env var exists because we are inheriting environment
    SC_TEST_EXPECT(out.containsString("PATH="));
    //! [ProcessEnvironmentNewVar]
}

void SC::ProcessTest::processEnvironmentRedefineParentVar()
{
    //! [ProcessEnvironmentRedefine]
    Process process(commandArena.toSpan(), environmentArena.toSpan());
    // This child process will inherit parent environment variables but we re-define PATH
    SC_TEST_EXPECT(process.setEnvironment("PATH", "/usr/sane_cpp_binaries"));
    String output;
    // Spawn the child process writing all env variables as KEY=VALUE\n to stdout, redirected to output
    SC_TEST_EXPECT(spawnChildAndPrintEnvironmentVars(process, output));
    // PATH env var has been re-defined
    StringView out = output.view();
    SC_TEST_EXPECT(out.containsString("PATH=/usr/sane_cpp_binaries"));
    //! [ProcessEnvironmentRedefine]
}

void SC::ProcessTest::processEnvironmentDisableInheritance()
{
    //! [ProcessEnvironmentDisableInheritance]
    Process process(commandArena.toSpan(), environmentArena.toSpan());
    process.inheritParentEnvironmentVariables(false);
    String output;
    // Spawn the child process writing all env variables as KEY=VALUE\n to stdout, redirected to output
    SC_TEST_EXPECT(spawnChildAndPrintEnvironmentVars(process, output));
    // PATH env var doesn't exist because of Process::inheritParentEnvironmentVariables(false)
    StringView out = output.view();
    SC_TEST_EXPECT(not out.containsString("PATH="));
    //! [ProcessEnvironmentDisableInheritance]
}

void SC::ProcessTest::processFork()
{
    //! [ProcessFork]
    // Cross-platform lightweight clone of current process, sharing memory
    // but keeping any modification after clone "private" (Copy-On-Write).
    // Achieved using "fork" on Posix and "RtlCloneUserProcess" on Windows.
    StringView sharedTag = "INITIAL";
    StringView parentTag = "PARENT";
    StringView saveFile  = "ForkSaveFile.txt";

    // The string will be duplicated using Copy-On-Write (COW)
    String shared = sharedTag;

    // CLONE current process, starting child fork in Suspended state
    // Forked process will be terminated by ProcessFork destructor
    ProcessFork fork;
    SC_TEST_EXPECT(fork.fork(ProcessFork::Suspended));

    // After fork program must check if it's on fork or parent side
    switch (fork.getSide())
    {
    case ProcessFork::ForkChild: {
        report.console.printLine("FORKED process");
        report.console.print("FORKED Shared={0}\n", shared.view());

        // Write the "shared" memory snapshot to the file system
        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
        SC_TEST_EXPECT(fs.writeString(saveFile, shared.view()));

        // Send (as a signal) modified string contents back to Parent
        SC_TEST_EXPECT(fork.getWritePipe().write({shared.view().toCharSpan()}));
    }
    break;
    case ProcessFork::ForkParent: {
        report.console.printLine("PARENT process");
        // Check initial state to be "INITIAL" and modify shared = "PARENT"
        report.console.print("PARENT Shared={0}\n", shared.view());
        SC_TEST_EXPECT(shared == sharedTag and "PARENT");
        shared = parentTag;

        // Resume suspended fork verifying that on its side shared == "INITIAL"
        SC_TEST_EXPECT(fork.resumeChildFork());
        char       string[255] = {0};
        Span<char> received;
        SC_TEST_EXPECT(fork.getReadPipe().read(string, received));
        StringView stringFromFork(received, true, StringEncoding::Ascii);
        report.console.print("PARENT received={0}\n", stringFromFork);
        SC_TEST_EXPECT(stringFromFork == sharedTag);

        // Check creation of "save file" by fork and verify its content too
        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
        String savedData = StringEncoding::Ascii;
        SC_TEST_EXPECT(fs.read(saveFile, savedData));
        SC_TEST_EXPECT(savedData == sharedTag);
        SC_TEST_EXPECT(fs.removeFile(saveFile));

        // Optionally wait for child process to exit and check its status
        SC_TEST_EXPECT(fork.waitForChild());
        SC_TEST_EXPECT(fork.getExitStatus() == 0);
    }
    break;
    }
    //! [ProcessFork]
}

SC::Result SC::ProcessTest::quickSheet()
{
    // clang-format off
    SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
    //! [ProcessQuickSheetSnippet]
// 1. Execute child process (launch and wait for it to fully execute)
Process().exec({"cmd-exe", "-h"});
//--------------------------------------------------------------------------
// 2. Execute child process, redirecting stdout to a string
SmallString<256> output; // could be also just String
Process().exec({"where-exe", "winver"}, output);
//--------------------------------------------------------------------------
// 3. Launch a child process and explicitly wait for it to finish execution
Process process;
// This is equivalent to process.exec({"1s", "-l"))
process.launch({"ls", "-l"});
//
// ...
// Here you can do I/0 to and from the spawned process
// ...
process.waitForExitSync();
//--------------------------------------------------------------------------
// 4. Execute child process, filling its stdin with a StringView
// This is equivalent of shell command: `echo "child proc" | grep process`
Process().exec({"grep", "process"}, Process::StdOut::Inherit(), "child proc");
//--------------------------------------------------------------------------
// 5. Read process output using a pipe, using launch + waitForExitSync
Process process5;
PipeDescriptor outputPipe;
process5.launch({"executable.exe", "—argument1", "-argument2"}, outputPipe);
String output5 = StringEncoding::Ascii; // Could also use SmallString<N>
outputPipe.readPipe.readUntilEOF(output5);
process5.waitForExitSync(); // call process-getExitStatus() for status code
//--------------------------------------------------------------------------
// 6. Executes two processes piping p1 output to p2 input
Process p1, p2;
ProcessChain chain;
chain.pipe(p1, {"echo", "Salve\nDoctori"});
chain.pipe(p2, {"grep", "Doc"});
// Read the output of the last process in the chain
String output6;
chain.exec(output6);
SC_ASSERT_RELEASE(output == "Doctori\n");
//--------------------------------------------------------------------------
// 7. Set an environment var and current directory for child process 
Process process7;
// This child process7 will inherit parent environment variables plus NewEnvVar
SC_TEST_EXPECT(process7.setEnvironment("NewEnvVar", "SomeValue"));
// This child process7 will inherit parent environment variables but we re-define PATH
SC_TEST_EXPECT(process7.setEnvironment("PATH", "/usr/sane_cpp_binaries"));
// Set the current working directory
SC_TEST_EXPECT(process7.setWorkingDirectory("/usr/home"));
    //! [ProcessQuickSheetSnippet]
    SC_COMPILER_WARNING_POP;
    // clang-format on
    return Result(true);
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
    Process process(commandArena.toSpan(), environmentArena.toSpan());
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
