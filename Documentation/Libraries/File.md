@page library_file File

@brief 🟩 Synchronous Disk File I/O

[SaneCppFile.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFile.h) is a library implementing synchronous I/O operations on files and pipes.  

[TOC]

# Dependencies
- Dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

![Dependency Graph](File.svg)


# Features
| SC::FileDescriptor                        | @copybrief SC::FileDescriptor                         |
|:------------------------------------------|:------------------------------------------------------|
| SC::FileDescriptor::read                  | @copybrief SC::FileDescriptor::read                   |
| SC::FileDescriptor::write                 | @copybrief SC::FileDescriptor::write                  |
| SC::FileDescriptor::seek                  | @copybrief SC::FileDescriptor::seek                   |
| SC::FileDescriptor::open                  | @copybrief SC::FileDescriptor::open                   |
| SC::FileDescriptor::openStdInDuplicate    | @copybrief SC::FileDescriptor::openStdInDuplicate     |
| SC::FileDescriptor::openStdOutDuplicate   | @copybrief SC::FileDescriptor::openStdOutDuplicate    |
| SC::FileDescriptor::openStdErrDuplicate   | @copybrief SC::FileDescriptor::openStdErrDuplicate    |
| SC::FileDescriptor::readUntilEOF          | @copybrief SC::FileDescriptor::readUntilEOF           |

| SC::PipeDescriptor                | @copybrief SC::PipeDescriptor                 |
|:----------------------------------|:----------------------------------------------|
| SC::PipeDescriptor::readPipe      | @copybrief SC::PipeDescriptor::readPipe       |
| SC::PipeDescriptor::writePipe     | @copybrief SC::PipeDescriptor::writePipe      |

| SC::NamedPipeServer                       | @copybrief SC::NamedPipeServer                        |
|:------------------------------------------|:------------------------------------------------------|
| SC::NamedPipeServer::create               | @copybrief SC::NamedPipeServer::create                |
| SC::NamedPipeServer::accept               | @copybrief SC::NamedPipeServer::accept                |
| SC::NamedPipeServer::close                | @copybrief SC::NamedPipeServer::close                 |
| SC::NamedPipeServerOptions                | @copybrief SC::NamedPipeServerOptions                 |
| SC::NamedPipeClient                       | @copybrief SC::NamedPipeClient                        |
| SC::NamedPipeClient::connect              | @copybrief SC::NamedPipeClient::connect               |
| SC::NamedPipeClientOptions                | @copybrief SC::NamedPipeClientOptions                 |
| SC::NamedPipeName                         | @copybrief SC::NamedPipeName                          |
| SC::NamedPipeName::build                  | @copybrief SC::NamedPipeName::build                   |
| SC::NamedPipeNameOptions                  | @copybrief SC::NamedPipeNameOptions                   |

# Status
🟩 Usable  
This library has a relatively limited scope and it should not need many additional features compared to now.   
Will consider bumping to Complete in the future.

# Blog

Some relevant blog posts are:

- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)

# Description
SC::FileDescriptor object can be created by SC::FileDescriptor::open-ing a path on file system and it can be SC::FileDescriptor::read or SC::FileDescriptor::write.  
Also non-blocking mode can be controlled with SC::FileDescriptor::setBlocking.  
A file can be marked as inheritable with SC::FileDescriptor::setInheritable so that in can be accessed by child processes. 
SC::PipeDescriptor creates a pipe for InterProcess communication.  
A pipe has read and write SC::FileDescriptor endpoints and it's used by [Process](@ref library_process) library to redirect standard input, output or error to other processes.  
It can also be used to read or write the standard input, output or error from current process into a binary buffer or a string (as done by SC::ProcessChain::readStdOutUntilEOFSync or other similar methods).

SC::NamedPipeServer and SC::NamedPipeClient provide named endpoint creation and connection.
Accepted/connected named pipe endpoints are exposed as SC::PipeDescriptor, so they can be used with the same read/write APIs and with [Async](@ref library_async) / [Async Streams](@ref library_async_streams) compositions.
SC::NamedPipeName::build can be used to compose platform-native endpoint names from a logical pipe name, avoiding platform-specific `#if` path conventions at call sites.

@copydetails SC::FileDescriptor

# Roadmap

🟦 Complete Features:
- None for now

💡 Unplanned Features:
- None for now

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 144			| 172		| 316	|
| Sources   | 1087			| 173		| 1260	|
| Sum       | 1231			| 345		| 1576	|
