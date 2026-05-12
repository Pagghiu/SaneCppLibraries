@page library_await Await

@brief 🟥 C++20 coroutine layer over Async

[TOC]

[SaneCppAwait.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAwait.h) is a draft
library exploring `co_await` syntax on top of [Async](@ref library_async).

@warning
This library is currently **Draft**. The API is intentionally small and may change as the coroutine model is refined.
Use it to experiment with the current direction, not as a stable public API yet.

# Dependencies
- Dependencies: [Async](@ref library_async)
- All dependencies: [Async](@ref library_async), [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Await.svg)


# Design Intent

`Await` is not a replacement for `AsyncEventLoop`. It wraps an existing `AsyncEventLoop&` with `AwaitEventLoop` and keeps
the same lifetime expectations: sockets, buffers, requests, output objects, and tasks must remain valid while operations
are active.

The goal is to let the compiler generate the callback state machine while preserving the Sane C++ style:

- coroutine functions return `AwaitTask`;
- coroutine completion returns plain `Result`;
- extra outputs are passed through explicit caller-provided objects;
- callback-style `SC::Async` code can coexist on the same event loop;
- coroutine frame allocation can be routed through caller-owned `AwaitArena` storage.

# Features

| Await API                                      | Description                                    |
|:-----------------------------------------------|:-----------------------------------------------|
| [AwaitEventLoop](@ref SC::AwaitEventLoop)      | @copybrief SC::AwaitEventLoop                  |
| [AwaitTask](@ref SC::AwaitTask)                | @copybrief SC::AwaitTask                       |
| [AwaitArena](@ref SC::AwaitArena)              | @copybrief SC::AwaitArena                      |
| [AwaitSleepAwaiter](@ref SC::AwaitSleepAwaiter)| @copybrief SC::AwaitSleepAwaiter               |
| [AwaitSocketAcceptAwaiter](@ref SC::AwaitSocketAcceptAwaiter) | @copybrief SC::AwaitSocketAcceptAwaiter |
| [AwaitSocketConnectAwaiter](@ref SC::AwaitSocketConnectAwaiter) | @copybrief SC::AwaitSocketConnectAwaiter |
| [AwaitSocketSendAwaiter](@ref SC::AwaitSocketSendAwaiter) | @copybrief SC::AwaitSocketSendAwaiter     |
| [AwaitSocketSendToAwaiter](@ref SC::AwaitSocketSendToAwaiter) | @copybrief SC::AwaitSocketSendToAwaiter |
| [AwaitSocketSendAllAwaiter](@ref SC::AwaitSocketSendAllAwaiter) | @copybrief SC::AwaitSocketSendAllAwaiter |
| [AwaitSocketReceiveAwaiter](@ref SC::AwaitSocketReceiveAwaiter) | @copybrief SC::AwaitSocketReceiveAwaiter |
| [AwaitSocketReceiveFromAwaiter](@ref SC::AwaitSocketReceiveFromAwaiter) | @copybrief SC::AwaitSocketReceiveFromAwaiter |
| [AwaitFileReadAwaiter](@ref SC::AwaitFileReadAwaiter) | @copybrief SC::AwaitFileReadAwaiter       |
| [AwaitFileWriteAwaiter](@ref SC::AwaitFileWriteAwaiter) | @copybrief SC::AwaitFileWriteAwaiter     |
| [AwaitFileSendAwaiter](@ref SC::AwaitFileSendAwaiter) | @copybrief SC::AwaitFileSendAwaiter       |
| [AwaitFileSystemOperationAwaiter](@ref SC::AwaitFileSystemOperationAwaiter) | @copybrief SC::AwaitFileSystemOperationAwaiter |
| [AwaitTaskTimeoutAwaiter](@ref SC::AwaitTaskTimeoutAwaiter) | @copybrief SC::AwaitTaskTimeoutAwaiter |
| [AwaitLoopWorkAwaiter](@ref SC::AwaitLoopWorkAwaiter) | @copybrief SC::AwaitLoopWorkAwaiter       |

# Example

```cpp
AwaitTask sendAndReceive(AwaitEventLoop& await, const SocketDescriptor& sender, const SocketDescriptor& receiver)
{
    const char sendBuffer[]      = {1, 2, 3};
    char       receiveBuffer[16] = {};

    AwaitSocketSendResult sendResult;
    SC_CO_TRY(co_await await.sendAll(sender, {sendBuffer, sizeof(sendBuffer)}, &sendResult));

    AwaitSocketReceiveResult receiveResult;
    SC_CO_TRY(co_await await.receive(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));

    co_return Result(true);
}
```

# Status

🟥 Draft

The current proof of concept supports:

- `sleep()`;
- socket `accept()` and `connect()`;
- socket `send()`, `sendAll()`, and `receive()`;
- datagram socket `sendTo()` and `receiveFrom()`;
- file `fileRead()`, `fileWrite()`, and `fileSend()`;
- selected filesystem operations: `fsOpen()`, `fsClose()`, `fsCopyFile()`, `fsCopyDirectory()`, `fsRename()`,
  `fsRemoveEmptyDirectory()`, and `fsRemoveFile()`;
- background `loopWork()`;
- task cancellation for currently suspended operations;
- awaiting explicitly spawned child tasks;
- child task timeout with `waitFor()`;
- optional arena-backed coroutine frame allocation.

The draft is tested by `SCAwaitTest`, which is separate from `SCTest` because it requires C++20 and the standard
coroutine header.

# Details

@copydetails group_await

## AwaitEventLoop

@copydoc SC::AwaitEventLoop

## AwaitTask

@copydoc SC::AwaitTask

## AwaitArena

@copydoc SC::AwaitArena

# Memory allocation

`AwaitArena` can hold coroutine frames in caller-provided storage. If an `AwaitEventLoop` is constructed without an
arena, the draft falls back to standard nothrow coroutine allocation.

Arena-backed frames are not individually freed. The caller must only reset the arena after all tasks using it have been
destroyed.

# Roadmap

🟥 Draft Features:

- Add the remaining `Async` operations that map cleanly to one-shot awaiters.
- Expand cancellation semantics and edge-case coverage for every awaiter, including parent cancellation while waiting on `waitFor()`.
- Clarify `AsyncFileSystemOperation::read()` and `write()` handle ownership before adding Await wrappers for them.
- Decide if arena allocation should become mandatory for production use.
- Investigate no-stdlib coroutine support.
- Validate exception-disabled compiler modes across platforms.
- Explore structured child tasks / task groups.

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 504			| 185		| 689	|
| Sources   | 1039			| 232		| 1271	|
| Sum       | 1543			| 417		| 1960	|
