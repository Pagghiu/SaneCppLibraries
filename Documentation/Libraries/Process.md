@page library_process Process

@brief 🟩 Create child processes and chain them (also usable with [Async](@ref library_async) library)

[TOC]

Process allows launching, chaining input and output, setting working directory and environment variables of child processes.

# Quick Sheet

```cpp
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
Process process;
PipeDescriptor outputPipe;
process.launch({"executable.exe", "—argument1", "-argument2"}, outputPipe);
String output = StringEncoding::Ascii; // Could also use SmallString<N>
outputPipe.readPipe.readUntilEOF(output);
process.waitForExitSync(); // call process-getExitStatus() for status code
//--------------------------------------------------------------------------
// 6. Executes two processes piping p1 output to p2 input
Process p1, p2;
ProcessChain chain;
chain.pipe(p1, {"echo", "Salve\nDoctori"});
chain.pipe(p2, {"grep", "Doc"});
// Read the output of the last process in the chain
String output;
chain.exec(output);
SC_ASSERT_RELEASE(output == "Doctori\n");
//--------------------------------------------------------------------------
// 7. Set an environment var and current directory for child process 
Process process;
// This child process will inherit parent environment variables plus NewEnvVar
SC_TEST_EXPECT(process.setEnvironment("NewEnvVar", "SomeValue"));
// This child process will inherit parent environment variables but we re-define PATH
SC_TEST_EXPECT(process.setEnvironment("PATH", "/usr/sane_cpp_binaries"));
// Set the current working directory
SC_TEST_EXPECT(process.setWorkingDirectory("/usr/home"));
```

# Features
| Class                     | Description
|:--------------------------|:----------------------------------|
| SC::Process               | @copybrief SC::Process            |
| SC::ProcessChain          | @copybrief SC::ProcessChain       |
| SC::ProcessEnvironment    | @copybrief SC::ProcessEnvironment |

# Status
🟩 Usable  
Library is being used in [SC::Plugin](@ref library_plugin) and in [SC::Tools](@ref page_tools).

# Description

The SC::Process class is used when handling a process in isolation, while the SC::ProcessChain is used when there is need to chain inputs and outputs of multiple processes together.

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.06 - Posix fork](https://www.youtube.com/watch?v=-OiVELMxL6Q)

# Blog

Some relevant blog posts are:

- [March 2024 Update](https://pagghiu.github.io/site/blog/2024-03-27-SaneCppLibrariesUpdate.html)
- [April 2024 Update](https://pagghiu.github.io/site/blog/2024-04-27-SaneCppLibrariesUpdate.html)

## Process
@copydoc SC::Process

## ProcessChain
@copydoc SC::ProcessChain

## ProcessEnvironment
@copydoc SC::ProcessEnvironment

# Roadmap

🟦 Complete Features:
- To be defined

💡 Unplanned Features:
- None so far
