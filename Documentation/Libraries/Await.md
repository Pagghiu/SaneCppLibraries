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
| [AwaitLoopWakeUp](@ref SC::AwaitLoopWakeUp) | @copybrief SC::AwaitLoopWakeUp           |
| [AwaitLoopWakeUpAwaiter](@ref SC::AwaitLoopWakeUpAwaiter) | @copybrief SC::AwaitLoopWakeUpAwaiter |
| [AwaitFileReadAwaiter](@ref SC::AwaitFileReadAwaiter) | @copybrief SC::AwaitFileReadAwaiter       |
| [AwaitFileWriteAwaiter](@ref SC::AwaitFileWriteAwaiter) | @copybrief SC::AwaitFileWriteAwaiter     |
| [AwaitFileSendAwaiter](@ref SC::AwaitFileSendAwaiter) | @copybrief SC::AwaitFileSendAwaiter       |
| [AwaitFilePollAwaiter](@ref SC::AwaitFilePollAwaiter) | @copybrief SC::AwaitFilePollAwaiter       |
| [AwaitFileSystemOperationAwaiter](@ref SC::AwaitFileSystemOperationAwaiter) | @copybrief SC::AwaitFileSystemOperationAwaiter |
| [AwaitTaskGroup](@ref SC::AwaitTaskGroup)       | @copybrief SC::AwaitTaskGroup                  |
| [AwaitTaskGroupWaitAllAwaiter](@ref SC::AwaitTaskGroupWaitAllAwaiter) | @copybrief SC::AwaitTaskGroupWaitAllAwaiter |
| [AwaitTaskGroupWaitAnyAwaiter](@ref SC::AwaitTaskGroupWaitAnyAwaiter) | @copybrief SC::AwaitTaskGroupWaitAnyAwaiter |
| [AwaitProcessExitAwaiter](@ref SC::AwaitProcessExitAwaiter) | @copybrief SC::AwaitProcessExitAwaiter |
| [AwaitSignalAwaiter](@ref SC::AwaitSignalAwaiter) | @copybrief SC::AwaitSignalAwaiter         |
| [AwaitTaskSpawnAwaiter](@ref SC::AwaitTaskSpawnAwaiter) | @copybrief SC::AwaitTaskSpawnAwaiter     |
| [AwaitTaskTimeoutAwaiter](@ref SC::AwaitTaskTimeoutAwaiter) | @copybrief SC::AwaitTaskTimeoutAwaiter |
| [AwaitLoopWorkAwaiter](@ref SC::AwaitLoopWorkAwaiter) | @copybrief SC::AwaitLoopWorkAwaiter       |

# Example

```cpp
AwaitTask echoServer(AwaitEventLoop& await, const SocketDescriptor& serverSocket, SocketDescriptor& accepted)
{
    char                     receiveBuffer[64] = {};
    AwaitSocketReceiveResult received;

    SC_CO_TRY(co_await await.accept(serverSocket, accepted));
    SC_CO_TRY(co_await await.receive(accepted, {receiveBuffer, sizeof(receiveBuffer)}, received));
    SC_CO_TRY(co_await await.sendAll(accepted, received.data));

    co_return Result(true);
}

AwaitTask echoClient(AwaitEventLoop& await, const SocketDescriptor& client, SocketIPAddress address,
                     Span<char> replyBuffer, AwaitSocketReceiveResult& reply)
{
    const char message[] = "niche readable await";

    SC_CO_TRY(co_await await.connect(client, address));
    SC_CO_TRY(co_await await.sendAll(client, {message, sizeof(message) - 1}));
    SC_CO_TRY(co_await await.receive(client, replyBuffer, reply));

    co_return Result(true);
}

AwaitTask echoConversation(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                           const SocketDescriptor& client, SocketIPAddress address,
                           SocketDescriptor& accepted, Span<char> replyBuffer,
                           AwaitSocketReceiveResult& reply)
{
    AwaitTask server = echoServer(await, serverSocket, accepted);
    AwaitTask clientTask = echoClient(await, client, address, replyBuffer, reply);

    AwaitTask*     storage[2] = {};
    AwaitTaskGroup group(await, storage);
    SC_CO_TRY(group.spawn(server));
    SC_CO_TRY(group.spawn(clientTask));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}
```

The reply buffer is supplied by the caller because `Await` keeps the same stable object and buffer lifetime expectations
as `Async`.

A complete console version of this flow lives in `Examples/AwaitEcho`.

# Status

🟥 Draft

The current proof of concept supports:

- `sleep()`;
- socket `accept()` and `connect()`;
- socket `send()`, scatter/gather `send()`, `sendAll()`, and `receive()`;
- datagram socket `sendTo()`, scatter/gather `sendTo()`, and `receiveFrom()`;
- loop wake-up waiting with `AwaitLoopWakeUp`;
- file `fileRead()`, `fileWrite()`, scatter/gather `fileWrite()`, `fileSend()`, and `filePoll()`;
- selected filesystem operations: `fsOpen()`, `fsClose()`, `fsRead()`, `fsWrite()`, `fsCopyFile()`,
  `fsCopyDirectory()`, `fsRename()`, `fsRemoveEmptyDirectory()`, and `fsRemoveFile()`;
- background `loopWork()`;
- process exit waiting with `processExit()`;
- one-shot signal waiting with `signal()`;
- task cancellation for currently suspended operations, including several socket, file polling, wake-up, and task-group
  waits covered by tests;
- awaiting explicitly spawned child tasks, or starting and awaiting one with `spawnAndWait()`;
- structured `AwaitTaskGroup` waiting with caller-provided task pointer storage, `waitAll()`, and `waitAny()`;
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

# Lifetime rules

Awaiters keep the same stable-object rules as `Async`: sockets, file descriptors, buffers, result objects, wake-up
objects, and child tasks must stay alive while the operation is active.

Result objects may contain spans into caller-provided buffers. For example, `AwaitSocketReceiveResult::data` and
`AwaitFileReadResult::data` point into the receive/read buffer passed to the awaiter. Keep those buffers alive until
after the result has been inspected or copied elsewhere.

# Task groups

`AwaitTaskGroup` stores caller-owned task pointers in caller-provided storage. Its default cancellation policy is
structured: cancelling the parent task while it is suspended in `waitAll()` or `waitAny()` cancels active children before
the parent completes. `AwaitTaskGroupCancelPolicy::LeaveChildrenRunning` exists for advanced cases where child tasks
outlive the waiting parent.

`waitAny()` defaults to `AwaitTaskGroupWaitAnyPolicy::CancelRemaining`, so stack-owned child tasks are not left active
after the first child completes. Use `LeaveRemainingRunning` only when pending children have an explicitly managed
lifetime.

# Memory allocation

`AwaitArena` can hold coroutine frames in caller-provided storage. The draft currently supports two allocation modes:

- Passing an arena to `AwaitEventLoop` gives no-allocation coroutine frame storage for production-style Sane C++ usage.
- Omitting the arena keeps experiments ergonomic and falls back to standard nothrow coroutine allocation.

Arena-backed frames are not individually freed. The caller must only reset the arena after all tasks using it have been
destroyed.

`AwaitEventLoop::hasArena()` can be used by tests and examples to make the selected mode explicit. The intended
direction is:

- examples and production-style code should pass an arena;
- tests may cover both arena and no-arena modes;
- a future production gate may require an arena without removing the no-arena draft path immediately.

# Roadmap

🟥 Draft Features:

- Add the remaining `Async` operations that map cleanly to one-shot awaiters.
- Expand cancellation semantics and edge-case coverage for filesystem and thread-pool-backed awaiters.
- Expand task group helpers with result aggregation helpers and more policy tests.
- Decide how to enforce arena-backed allocation in production builds.
- Investigate no-stdlib coroutine support.
- Validate exception-disabled compiler modes across platforms.

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 730			| 253		| 983	|
| Sources   | 1643			| 333		| 1976	|
| Sum       | 2373			| 586		| 2959	|
