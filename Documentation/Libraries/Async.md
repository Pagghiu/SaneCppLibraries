@page library_async Async

@brief 🟨 Cross-platform completion-based asynchronous I/O

[TOC]

[SaneCppAsync.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAsync.h) is a
completion-based event loop for files, sockets, timers, processes, filesystem operations, background work, and signals.
Its request bookkeeping and payload storage are caller-owned; creating the loop and starting operations can still create
operating-system resources such as queues, descriptors, and threads.

# Dependencies
- Dependencies: [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)
- All dependencies: [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Async.svg)


# What Async Is For

Synchronous I/O can leave a thread waiting for a file, socket, timer, or child process. `Async` lets one thread submit
many such operations and dispatch their completions from a single `AsyncEventLoop`. It presents the same
completion-based model over IOCP on Windows, kqueue on macOS, and io_uring or epoll on Linux.

Use `Async` when you need:

- one cross-platform loop for sockets, files, timers, signals, processes, filesystem operations, and background work;
- explicit control over when requests are submitted, the operating system is polled, and callbacks are dispatched;
- integration with another loop, such as an immediate-mode GUI or an application-owned main loop;
- bounded, caller-owned request and buffer storage with no allocation inside the library;
- callbacks rather than a coroutine or fiber runtime.

For sequential stream processing, see [AsyncStreams](@ref library_async_streams). For C++20 `co_await` syntax over a
subset of these operations, see [Await](@ref library_await). For stackful tasks that suspend on this event loop, see
[FibersAsync](@ref library_fibers_async).

# Mental Model: Requests Live Until Completion

Every operation is an object derived from `AsyncRequest`: for example `AsyncSocketReceive`, `AsyncFileRead`, or
`AsyncLoopTimeout`. The caller fills the request, installs its callback, and starts it on an `AsyncEventLoop`. The loop
submits it to the platform backend and later invokes the callback while dispatching completions.

The request object and all memory referenced by it must stay at stable addresses until its callback runs. The event loop
links requests through intrusive fields in `AsyncRequest`; it does not copy them or allocate replacement state. Buffers
must remain valid for the same period.

Once `start()` succeeds, completion—including an operating-system error—is reported through the callback. A request
whose start fails was not accepted. Explicitly stopping a request suppresses its normal callback. Until the optional
stop callback runs, the request is still owned by the loop and its storage cannot be reused; without that callback, the
caller must otherwise drive teardown to completion before destroying it.

A completion callback may call `result.reactivateRequest(true)` to submit the same request again. This is useful for a
repeating timer, a receive loop, or another long-lived operation, but it also extends the request and buffer lifetime to
the next completion.

# Driving The Event Loop

The smallest complete shape is: create a loop, create caller-owned requests, start them, and run until no work remains.
This example is compiled as part of `AsyncTest`:

@snippet Tests/Libraries/Async/AsyncTest.cpp AsyncEventLoopSnippet

There are three convenience run modes:

- `run()` blocks and dispatches until every request has completed or been stopped.
- `runOnce()` blocks until at least one request makes progress, then dispatches ready completions.
- `runNoWait()` submits and dispatches currently available work without waiting.

Applications that already own their main loop can separate the three phases with `submitRequests()`, `blockingPoll()`,
and `dispatchCompletions()`. `AsyncEventLoopMonitor` goes further: one thread can monitor the OS backend while another
thread stops monitoring and dispatches completions. This separation is the important integration seam—callbacks still
run on the thread that dispatches them, not on an arbitrary kernel thread.

`run()` normally returns after counted work drains, but `interrupt()` can make it return early and
`excludeFromActiveCount()` can keep a long-lived request from holding it open. Neither facility releases an active
request: it must still complete, be stopped, or be released when the loop closes.

# Timers And Cross-Thread Wake-Ups

`AsyncLoopTimeout` schedules an absolute or relative timeout. Its callback can alter the request and reactivate it,
turning the same stable object into a repeating timer:

@snippet Tests/Libraries/Async/AsyncTest.cpp AsyncLoopTimeoutSnippet

`AsyncLoopWakeUp` is the explicit bridge from another thread into the event loop. Waking a request through the event
loop is thread-safe; the callback is delivered by the thread dispatching the event loop:

@snippet Tests/Libraries/Async/AsyncTest.cpp AsyncLoopWakeUpSnippet1

Use `AsyncLoopWork` for work submitted to a caller-provided `ThreadPool`, and `AsyncExternalCompletion` when an external
API already has its own asynchronous completion mechanism that needs to re-enter the loop.

# Socket I/O

The loop can create asynchronous TCP and UDP sockets and operate on `SocketDescriptor` values from the
[Socket](@ref library_socket) library. TCP accept/connect and stream send/receive are separate request types; UDP uses
send-to/receive-from requests that carry an address.

A receive request owns no payload memory: the destination buffer in this compiled example remains in scope until the
callback finishes. The callback may reactivate the request after consuming data to continue the receive loop.

@snippet Tests/Libraries/Async/AsyncTest.cpp AsyncSocketReceiveSnippet

Send and receive operations may complete with fewer bytes than requested. Applications must advance through their data
or use [AsyncStreams](@ref library_async_streams) when they want higher-level streaming and buffering behavior.

# File I/O And Blocking Backends

Files expose a portability distinction that the API keeps visible. Some backends can perform a file operation natively;
others require blocking work. For operations that may block, the caller supplies a `ThreadPool` and associates the
request with an `AsyncTaskSequence` through `executeOn()`.

`AsyncThreadPoolMode::NativePreferred` uses native support when available and the supplied pool where necessary.
`AsyncThreadPoolMode::ForceThreadPool` forces the pool even on a native-capable backend. The pool, its storage, the file
descriptor, request object, and buffers all remain caller-owned.

This source-backed example opens a regular file, reads it in chunks, and uses a task sequence where the backend needs
blocking work:

@snippet Tests/Libraries/Async/AsyncTest.cpp AsyncFileReadSnippet

When using explicit file offsets, prefer one contiguous buffer for portable code. Scatter/gather writes combined with
an offset need backend-specific validation before being treated as a portable idiom.

`AsyncFileReadiness` is different from file read/write: it waits for readiness on descriptors where the operating
system provides that concept. POSIX file readiness is exposed directly. Windows does not provide a portable equivalent
for ordinary file or pipe handles, and higher layers should not pretend otherwise.

The file request types also operate on compatible descriptor handles from neighboring libraries. That includes serial
ports from [SerialPort](@ref library_serial_port) and `PipeDescriptor` endpoints accepted or connected through
`NamedPipeServer` and `NamedPipeClient`; their framing, connection, and platform rules still come from those APIs rather
than from `Async`.

# Sequencing And Long-Lived Operations

`AsyncSequence` serializes requests by submitting the next request after the current one completes. It is useful when
operations share a descriptor or must preserve ordering. `AsyncTaskSequence` combines that ordering with a supplied
thread pool for potentially blocking work.

Long-lived sources such as `FileSystemWatcher` naturally remain callback streams. Their request and any bounded event
storage must remain owned by the application until the watcher is stopped. This is also why [Await](@ref library_await)
does not expose a watcher as a single awaitable operation.

The library also provides requests for child-process exit, signals, filesystem operations, and external completions.
See the [Async API group](@ref group_async) for the complete reference after choosing the relevant operation family.

# Platform Backends And Tradeoffs

The common request model maps to different operating-system facilities:

| Platform | Backend | Important consequence |
|----------|---------|-----------------------|
| Windows  | IOCP / `OVERLAPPED` | Completion-based socket and file I/O; some handle readiness concepts do not map portably. |
| macOS    | kqueue | Readiness and event notifications; blocking file work uses a supplied thread pool. |
| Linux    | direct io_uring, falling back to epoll | No `liburing` dependency; native file I/O is preferred when io_uring is available. |

The Linux backend first attempts direct io_uring setup and falls back to epoll when the kernel or runtime policy does
not permit it. Applications should depend on the `Async` semantics rather than assuming which backend was selected.

Caller-owned request and payload storage gives predictable ownership and makes bounded pools possible, but it is not
automatic lifetime management. A request accidentally moved, destroyed, or paired with a temporary buffer while active
is a programming error. `ArenaMap` from [Containers](@ref library_containers) can provide a preallocated bounded pool
of stable request objects when an application needs many operations.

# Status

🟨 MVP
This is usable but needs some more testing and a few more features.

# Videos

These videos record the implementation and integration work behind the library:

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

Development notes and design changes are recorded in the project updates:

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
- [February 2026 Update](https://pagghiu.github.io/site/blog/2026-02-28-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)
- [May 2026 Update](https://pagghiu.github.io/site/blog/2026-05-31-SaneCppLibrariesUpdate.html)
- [June 2026 Update](https://pagghiu.github.io/site/blog/2026-06-30-SaneCppLibrariesUpdate.html)

# Roadmap

🟩 Usable Features:
- More AsyncFileSystemOperations
- Async DNS Resolution

🟦 Complete Features:
- TTY with ANSI Escape Codes
- Signal handling (multi-watcher, cross-platform)

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Async`.
Single File counts
`SaneCppAsync.h`.
Standalone counts `SaneCppAsyncStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 1926   | 5842   | 7768  |
| Single File | 1646   | 6974   | 8620  |
| Standalone  | 6190   | 13279  | 19469 |
