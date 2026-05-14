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
| [AwaitSocketReceiveExactAwaiter](@ref SC::AwaitSocketReceiveExactAwaiter) | @copybrief SC::AwaitSocketReceiveExactAwaiter |
| [AwaitSocketReceiveLineAwaiter](@ref SC::AwaitSocketReceiveLineAwaiter) | @copybrief SC::AwaitSocketReceiveLineAwaiter |
| [AwaitSocketReceiveFromAwaiter](@ref SC::AwaitSocketReceiveFromAwaiter) | @copybrief SC::AwaitSocketReceiveFromAwaiter |
| [AwaitLoopWakeUp](@ref SC::AwaitLoopWakeUp) | @copybrief SC::AwaitLoopWakeUp           |
| [AwaitLoopWakeUpAwaiter](@ref SC::AwaitLoopWakeUpAwaiter) | @copybrief SC::AwaitLoopWakeUpAwaiter |
| [AwaitFileReadAwaiter](@ref SC::AwaitFileReadAwaiter) | @copybrief SC::AwaitFileReadAwaiter       |
| [AwaitFileReadUntilFullOrEOFAwaiter](@ref SC::AwaitFileReadUntilFullOrEOFAwaiter) | @copybrief SC::AwaitFileReadUntilFullOrEOFAwaiter |
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

Complete console examples live in:

- `Examples/AwaitEcho`, showing sockets, task groups, and arena-backed tasks.
- `Examples/AwaitDatagramPing`, showing UDP `sendTo()` / `receiveFrom()` request and reply flow.

# Socket send helpers

`send()` is the direct one-shot wrapper over `AsyncSocketSend`: it completes when the socket reports one send
operation, which may be smaller than the caller-provided data.

`sendAll()` is the higher-level stream helper: it reactivates the underlying send request until all caller-provided data
has been sent. When an `AwaitSocketSendResult*` is provided, `numBytes` reports the cumulative byte count.

Single-buffer data can be sent directly:

```cpp
const char message[] = "single buffer payload";
AwaitSocketSendResult sent;

SC_CO_TRY(co_await await.sendAll(socket, {message, sizeof(message) - 1}, &sent));
```

Scatter/gather data uses caller-owned span storage, preserving the same no-allocation shape as `Async`:

```cpp
const char       header[] = "body:";
Span<const char> buffers[] = {{header, sizeof(header) - 1}, body};
AwaitSocketSendResult sent;

SC_CO_TRY(co_await await.sendAll(socket, buffers, &sent));
```

# Status

🟥 Draft

The current proof of concept supports:

- `sleep()`;
- socket `accept()` and `connect()`;
- socket `send()`, scatter/gather `send()`, `sendAll()`, `receive()`, `receiveExact()`, and `receiveLine()`;
- datagram socket `sendTo()`, scatter/gather `sendTo()`, and `receiveFrom()`;
- loop wake-up waiting with `AwaitLoopWakeUp`;
- file `fileRead()`, offset `fileRead()`, `fileReadUntilFullOrEOF()`, `fileWrite()`, offset `fileWrite()`,
  scatter/gather `fileWrite()`, `fileSend()`, and POSIX `filePoll()`;
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

# Socket receive helpers

`receive()` is the direct one-shot wrapper over `AsyncSocketReceive`: it completes when the socket reports some data,
disconnect, or an error, and the returned `AwaitSocketReceiveResult::data` span may be smaller than the caller buffer.

`receiveExact()` is the higher-level stream helper: it reactivates the underlying receive request until the caller buffer
is full. If the peer disconnects before the buffer is full, it returns an error and, when an output result is provided,
the result object describes the partial data received so far.

`receiveLine()` is a no-allocation line helper for simple text protocols. It reads into caller-provided storage until
`\n`, trims a preceding `\r` from the reported line span, and fails if the buffer fills before the newline arrives.

# File helpers

`fileRead()` is the direct one-shot wrapper over `AsyncFileRead`: it may return fewer bytes than the caller buffer
holds.

`fileReadUntilFullOrEOF()` mirrors `FileDescriptor::readUntilFullOrEOF()` for coroutine code. It reactivates the
underlying read request until the caller buffer is full or EOF is reached, and reports the actually-read prefix through
`AwaitFileReadResult::data`.

`fileWrite()` does not need a separate `fileWriteAll()` helper today: `AsyncFileWrite` already keeps writing until the
provided single buffer or scatter/gather buffers are fully written, or returns an error.

# Helper placement

Thin convenience helpers currently live on `AwaitEventLoop` when they preserve the shape of one underlying `Async`
operation and only add a small, no-allocation loop over caller-provided storage. Examples are `sendAll()`,
`receiveExact()`, `receiveLine()`, and `fileReadUntilFullOrEOF()`.

If future helpers start carrying protocol state, buffering policy, parsing rules, or multiple stable objects, they should
move into explicit `Await*` helper structs instead of making `AwaitEventLoop` a grab bag.

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
- production builds can define `SC_AWAIT_REQUIRE_ARENA=1` to make coroutine frame allocation fail instead of falling
  back to standard allocation when no `AwaitArena` is discoverable from the coroutine parameters.

# Exceptions

`Await` does not use exceptions for control flow. The C++20 test and examples intentionally keep
`CompileFlags::enableExceptions` disabled, so macOS, Linux, and Windows validation covers the exception-disabled path.
`AwaitTask::Promise::unhandled_exception()` remains present because the C++ coroutine promise interface requires it when
compiling against the standard coroutine header.

# Roadmap

🟥 Draft Features:

- Add the remaining `Async` operations that map cleanly to one-shot awaiters.
- Expand cancellation semantics and edge-case coverage for filesystem and thread-pool-backed awaiters.
- Expand task group helpers with result aggregation helpers and more policy tests.
- Decide if `SC_AWAIT_REQUIRE_ARENA=1` should become the default for non-test builds.
- Investigate no-stdlib coroutine support.

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 730			| 253		| 983	|
| Sources   | 1643			| 333		| 1976	|
| Sum       | 2373			| 586		| 2959	|
