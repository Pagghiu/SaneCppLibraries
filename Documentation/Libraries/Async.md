@page library_async Async

@brief 🟨 Async I/O (files, sockets, timers, processes, fs events, threads wake-up)

[TOC]

[SaneCppAsync.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsync.h) is a multi-platform / event-driven asynchronous I/O library.  

@note
Check @ref library_async_streams for an higher level construct when streaming data

# Dependencies
- Dependencies: [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)
- All dependencies: [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Async.svg)


# Features

This is the list of supported async operations:

| Async Operation                                               | Description                               |
|---------------------------------------------------------------|-------------------------------------------|
| [AsyncSocketConnect](@ref SC::AsyncSocketConnect)             | @copybrief SC::AsyncSocketConnect         | 
| [AsyncSocketAccept](@ref SC::AsyncSocketAccept)               | @copybrief SC::AsyncSocketAccept          |
| [AsyncSocketSend](@ref SC::AsyncSocketSend)                   | @copybrief SC::AsyncSocketSend            |
| [AsyncSocketReceive](@ref SC::AsyncSocketReceive)             | @copybrief SC::AsyncSocketReceive         |
| [AsyncSocketSendTo](@ref SC::AsyncSocketSendTo)               | @copybrief SC::AsyncSocketSendTo          |
| [AsyncSocketReceiveFrom](@ref SC::AsyncSocketReceiveFrom)     | @copybrief SC::AsyncSocketReceiveFrom     |
| [AsyncFileRead](@ref SC::AsyncFileRead)                       | @copybrief SC::AsyncFileRead              |
| [AsyncFileWrite](@ref SC::AsyncFileWrite)                     | @copybrief SC::AsyncFileWrite             |
| [AsyncLoopTimeout](@ref SC::AsyncLoopTimeout)                 | @copybrief SC::AsyncLoopTimeout           |
| [AsyncLoopWakeUp](@ref SC::AsyncLoopWakeUp)                   | @copybrief SC::AsyncLoopWakeUp            |
| [AsyncLoopWork](@ref SC::AsyncLoopWork)                       | @copybrief SC::AsyncLoopWork              |
| [AsyncProcessExit](@ref SC::AsyncProcessExit)                 | @copybrief SC::AsyncProcessExit           |
| [AsyncFilePoll](@ref SC::AsyncFilePoll)                       | @copybrief SC::AsyncFilePoll              |
| [AsyncSequence](@ref SC::AsyncSequence)                       | @copybrief SC::AsyncSequence              |
| [AsyncFileSystemOperation](@ref SC::AsyncFileSystemOperation) | @copybrief SC::AsyncFileSystemOperation   |

# Details

@copydetails group_async

# Status
🟨 MVP  
This is usable but needs some more testing and a few more features.

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.07 - SC::Async Linux epoll 1/2](https://www.youtube.com/watch?v=4rC4aKCD0V8)
- [Ep.08 - SC::Async Linux epoll 2/2](https://www.youtube.com/watch?v=uCsGpJcF2oc)
- [Ep.10 - A Tour of SC::Async](https://www.youtube.com/watch?v=pIGosb2D2Ro)
- [Ep.11 - Linux Async I/O using io_uring (1 of 2)](https://www.youtube.com/watch?v=YR935rorb3E)
- [Ep.12 - Linux Async I/O using io_uring (2 of 2)](https://www.youtube.com/watch?v=CgYE0YrpHt0)
- [Ep.14 - Async file read and writes using Thread Pool](https://www.youtube.com/watch?v=WF9beKyEA_E)
- [Ep.16 - Implement SC::AsyncLoopWork](https://www.youtube.com/watch?v=huavEjzflHQ)
- [Ep.18 - BREAK SC::Async IO Event Loop](https://www.youtube.com/watch?v=3lbyx11qDxM)
- [Ep.20 - Pause Immediate Mode UI - Save CPU Time](https://www.youtube.com/watch?v=4acqdGcUQnE)
- [Ep.21 - Add Async IO to Immediate Mode GUI](https://www.youtube.com/watch?v=z7QaTa7drFo)

# Blog

Some relevant blog posts are:

- [April 2024 Update](https://pagghiu.github.io/site/blog/2024-04-27-SaneCppLibrariesUpdate.html)
- [May 2024 Update](https://pagghiu.github.io/site/blog/2024-05-31-SaneCppLibrariesUpdate.html)
- [June 2024 Update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html)
- [July 2024 Update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)
- [August 2024 Update](https://pagghiu.github.io/site/blog/2024-08-30-SaneCppLibrariesUpdate.html)
- [December 2024 Update](https://pagghiu.github.io/site/blog/2024-12-31-SaneCppLibrariesUpdate.html)
- [January 2025 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)
- [April 2025 Update](https://pagghiu.github.io/site/blog/2025-04-30-SaneCppLibrariesUpdate.html)
- [May 2025 Update](https://pagghiu.github.io/site/blog/2025-05-31-SaneCppLibrariesUpdate.html)
- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)
- [November 2025 Update](https://pagghiu.github.io/site/blog/2025-11-30-SaneCppLibrariesUpdate.html)
- [December 2025 Update](https://pagghiu.github.io/site/blog/2025-12-31-SaneCppLibrariesUpdate.html)
- [January 2026 Update](https://pagghiu.github.io/site/blog/2026-01-31-SaneCppLibrariesUpdate.html)

# Description
@copydetails SC::AsyncRequest

## AsyncEventLoop
@copydoc SC::AsyncEventLoop

### Run modes

Event loop can be run in different ways to allow integrated it in multiple ways in applications.

| Run mode                      | Description                               |
|:------------------------------|:------------------------------------------|
| SC::AsyncEventLoop::run       | @copydoc SC::AsyncEventLoop::run          |
| SC::AsyncEventLoop::runOnce   | @copydoc SC::AsyncEventLoop::runOnce      |
| SC::AsyncEventLoop::runNoWait | @copydoc SC::AsyncEventLoop::runNoWait    |


Alternatively user can explicitly use three methods to submit, poll and dispatch events.
This is very useful to integrate the event loop into applications with other event loops (for example GUI applications).

| Run mode                                  | Description                                       |
|:------------------------------------------|:--------------------------------------------------|
| SC::AsyncEventLoop::submitRequests        | @copydoc SC::AsyncEventLoop::submitRequests       |
| SC::AsyncEventLoop::blockingPoll          | @copydoc SC::AsyncEventLoop::blockingPoll         |
| SC::AsyncEventLoop::dispatchCompletions   | @copydoc SC::AsyncEventLoop::dispatchCompletions  |

## AsyncEventLoopMonitor
@copydoc SC::AsyncEventLoopMonitor

| Functions                                                         | Description                                                               |
|:------------------------------------------------------------------|:--------------------------------------------------------------------------|
| SC::AsyncEventLoopMonitor::startMonitoring                        | @copydoc SC::AsyncEventLoopMonitor::startMonitoring                       |
| SC::AsyncEventLoopMonitor::stopMonitoringAndDispatchCompletions   | @copydoc SC::AsyncEventLoopMonitor::stopMonitoringAndDispatchCompletions  |

## AsyncLoopTimeout
@copydoc SC::AsyncLoopTimeout

## AsyncLoopWakeUp
@copydoc SC::AsyncLoopWakeUp

## AsyncLoopWork
@copydoc SC::AsyncLoopWork

## AsyncProcessExit
@copydoc SC::AsyncProcessExit

## AsyncSocketAccept
@copydoc SC::AsyncSocketAccept

## AsyncSocketConnect
@copydoc SC::AsyncSocketConnect

## AsyncSocketSend
@copydoc SC::AsyncSocketSend

## AsyncSocketReceive
@copydoc SC::AsyncSocketReceive

## AsyncSocketSendTo
@copydoc SC::AsyncSocketSendTo

## AsyncSocketReceiveFrom
@copydoc SC::AsyncSocketReceiveFrom

## AsyncFileRead
@copydoc SC::AsyncFileRead

## AsyncFileWrite
@copydoc SC::AsyncFileWrite

## AsyncFilePoll
@copydoc SC::AsyncFilePoll

## AsyncSequence
@copydoc SC::AsyncSequence

## AsyncTaskSequence
@copydoc SC::AsyncTaskSequence

## AsyncFileSystemOperation
@copydoc SC::AsyncFileSystemOperation

# Implementation

Library abstracts async operations by exposing a completion based mechanism.
This mechanism currently maps on `kqueue` on macOS and `OVERLAPPED` on Windows.

It currently tries to dynamically load `io_uring` on Linux doing an `epoll` backend fallback in case `liburing` is not available on the system.
There is not need to link `liburing` because the library loads it dynamically and embeds the minimal set of `static` `inline` functions needed to interface with it.

The api works on file and socket descriptors, that can be obtained from the [File](@ref library_file) and [Socket](@ref library_socket) libraries.

## Memory allocation
The entire library is free of allocations, as it uses a double linked list inside SC::AsyncRequest.  
Caller is responsible for keeping AsyncRequest-derived objects memory stable until async callback is called.  
SC::ArenaMap from the [Containers](@ref library_containers) can be used to preallocate a bounded pool of Async objects.

# Roadmap

🟩 Usable Features:
- More AsyncFileSystemOperations
- Async DNS Resolution

🟦 Complete Features:
- TTY with ANSI Escape Codes
- Signal handling

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 756			| 728		| 1484	|
| Sources   | 5038			| 1195		| 6233	|
| Sum       | 5794			| 1923		| 7717	|
