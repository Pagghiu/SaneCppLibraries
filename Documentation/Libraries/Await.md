@page library_await Await

@brief 🟨 C++20 coroutine layer over Async

[TOC]

[SaneCppAwait.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAwait.h) is an
experimental library providing `co_await` syntax on top of [Async](@ref library_async).

@warning
This library is currently **MVP / Experimental**. It covers useful `Async` workflows, but the API may still change as
the coroutine model is refined. Use it for focused experiments and examples, not as a stable public API yet.

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
- coroutine frame allocation is explicit through a mandatory `AwaitAllocator`.

# Features

| Await API                                      | Description                                    |
|:-----------------------------------------------|:-----------------------------------------------|
| [AwaitEventLoop](@ref SC::AwaitEventLoop)      | @copybrief SC::AwaitEventLoop                  |
| [AwaitTask](@ref SC::AwaitTask)                | @copybrief SC::AwaitTask                       |
| [AwaitAllocator](@ref SC::AwaitAllocator)      | @copybrief SC::AwaitAllocator                  |
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
| [AwaitTaskRegistry](@ref SC::AwaitTaskRegistry) | @copybrief SC::AwaitTaskRegistry               |
| [AwaitTaskGroupResultSummary](@ref SC::AwaitTaskGroupResultSummary) | @copybrief SC::AwaitTaskGroupResultSummary |
| [AwaitTaskGroupWaitAllAwaiter](@ref SC::AwaitTaskGroupWaitAllAwaiter) | @copybrief SC::AwaitTaskGroupWaitAllAwaiter |
| [AwaitTaskGroupWaitAnyAwaiter](@ref SC::AwaitTaskGroupWaitAnyAwaiter) | @copybrief SC::AwaitTaskGroupWaitAnyAwaiter |
| [AwaitTaskRegistryWaitAllAwaiter](@ref SC::AwaitTaskRegistryWaitAllAwaiter) | @copybrief SC::AwaitTaskRegistryWaitAllAwaiter |
| [AwaitTaskRegistryWaitAnyAwaiter](@ref SC::AwaitTaskRegistryWaitAnyAwaiter) | @copybrief SC::AwaitTaskRegistryWaitAnyAwaiter |
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

- `Examples/AwaitBackgroundDigest`, showing ThreadPool-backed CPU work with `loopWork()` and caller-owned job state.
- `Examples/AwaitBackgroundJobs`, showing detached background coroutines through fixed caller-owned
  `AwaitTaskRegistry` slots.
- `Examples/AwaitFirstResponse`, racing caller-owned registry jobs with `waitAny()` and cancelling the slower response.
- `Examples/AwaitConfigReload`, showing `spawnAndWait()` for a single child coroutine that loads a config file.
- `Examples/AwaitDeadline`, showing a child coroutine deadline with `waitFor()` and cooperative cancellation.
- `Examples/AwaitEcho`, showing sockets, task groups, and fixed-allocator-backed tasks.
- `Examples/AwaitDatagramPing`, showing UDP `sendTo()` / `receiveFrom()` request and reply flow.
- `Examples/AwaitTaskGroupFiles`, showing Python `asyncio.TaskGroup`-style fan-out over two file reads while keeping
  task storage caller-owned.
- `Examples/AwaitCallbackBridge`, showing callback-style `Async` and coroutine-style `Await` sharing one
  caller-owned event loop during migration.
- `Examples/AwaitFileCourier`, showing a file copy followed by `fileSend()` over a socket.
- `Examples/AwaitFilePatch`, showing offset `fileWrite()` followed by `fileRead()` with caller-owned buffers.
- `Examples/AwaitLineProtocol`, showing a tiny CRLF text protocol built with `receiveLine()` and `sendAll()`.
- `Examples/AwaitManifestPreview`, showing bounded `fileReadUntilFullOrEOF()` into caller-owned preview storage.
- `Examples/AwaitProcessExitCodes`, showing concurrent child-process exit waits with fixed job storage.
- `Examples/AwaitThreadWakeUp`, showing another thread waking an Await coroutine through `AwaitLoopWakeUp`.

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

🟨 MVP / Experimental

Current support includes:

- `sleep()`;
- socket `accept()` and `connect()`;
- socket `send()`, scatter/gather `send()`, `sendAll()`, `receive()`, `receiveExact()`, and `receiveLine()`;
- datagram socket `sendTo()`, scatter/gather `sendTo()`, and `receiveFrom()`;
- loop wake-up waiting with `AwaitLoopWakeUp`;
- file `fileRead()`, offset `fileRead()`, `fileReadUntilFullOrEOF()`, `fileWrite()`, offset `fileWrite()`,
  scatter/gather `fileWrite()`, `fileSend()`, and POSIX `filePoll()`;
- serial descriptors through the existing file awaiters, because `SerialDescriptor` is a `FileDescriptor` and `Async`
  models serial I/O with `AsyncFileRead` / `AsyncFileWrite`;
- selected filesystem operations: `fsOpen()`, `fsClose()`, `fsRead()`, `fsWrite()`, `fsCopyFile()`,
  `fsCopyDirectory()`, `fsRename()`, `fsRemoveEmptyDirectory()`, and `fsRemoveFile()`;
- filesystem watching remains in [FileSystemWatcher](@ref library_file_system_watcher) rather than direct
  `AwaitEventLoop` awaiters, because it is a long-lived callback stream instead of a one-shot operation;
- background `loopWork()`;
- process exit waiting with `processExit()`;
- one-shot signal waiting with `signal()`;
- task cancellation for currently suspended operations, including several socket, file polling, wake-up, and task-group
  waits covered by tests;
- awaiting explicitly spawned child tasks, or starting and awaiting one with `spawnAndWait()`;
- structured `AwaitTaskGroup` waiting with caller-provided task pointer storage, `waitAll()`, and `waitAny()`;
- fixed-slot `AwaitTaskRegistry` ownership for detached/background tasks that are cleaned up explicitly;
- child task timeout with `waitFor()`;
- mandatory explicit coroutine frame allocation through `AwaitAllocator`.

`Await` is tested by `SCAwaitTest`, which is separate from `SCTest` because it requires C++20 and the standard
coroutine header. `Await` is available in the default standard-header-enabled mode and emits a clear diagnostic if
compiled with `SC_INCLUDE_STD_CPP=0`. Coroutine frame storage is always explicit through `AwaitAllocator`, so enabling
Await does not add a dependency on the Memory library.

# Details

@copydetails group_await

## AwaitEventLoop

@copydoc SC::AwaitEventLoop

`AwaitEventLoop` wraps an existing caller-owned `AsyncEventLoop&`. It does not create, close, or otherwise own the
underlying loop; callback-style `Async` requests and coroutine-style `Await` tasks can share the same loop as long as
their stable objects remain alive while active.

## AwaitTask

@copydoc SC::AwaitTask

## AwaitAllocator

@copydoc SC::AwaitAllocator

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

For offset writes, prefer a single contiguous buffer in portable examples. Combining scatter/gather file writes with an
explicit offset should be treated as backend-sensitive until the underlying `AsyncFileWrite` semantics are tightened.

# Filesystem watcher integration

`Await` intentionally does not expose direct filesystem watcher awaiters yet. `FileSystemWatcher` already models this as
a long-lived callback stream with stable `FileSystemWatcher::FolderWatcher` objects and an event-loop runner
(`FileSystemWatcherAsyncT<AsyncEventLoop>`). That shape is closer to an async iterator or channel than to the one-shot
`AsyncRequest` wrappers currently living on `AwaitEventLoop`.

For now, coroutine examples should keep using callback-style `FileSystemWatcher` on the same underlying
`AsyncEventLoop`, or introduce a small caller-owned adapter outside the core `AwaitEventLoop` surface when a concrete
workflow needs it. If a future helper is added, it should be an explicit `Await*` object carrying the watcher state and
caller-provided notification storage, not another thin method on `AwaitEventLoop`.

# Helper placement

Thin convenience helpers currently live on `AwaitEventLoop` when they preserve the shape of one underlying `Async`
operation and only add a small, no-allocation loop over caller-provided storage. Examples are `sendAll()`,
`receiveExact()`, `receiveLine()`, and `fileReadUntilFullOrEOF()`.

If future helpers start carrying protocol state, buffering policy, parsing rules, or multiple stable objects, they should
move into explicit `Await*` helper structs instead of making `AwaitEventLoop` a grab bag.

`spawnAndWait()` intentionally remains a convenience awaiter for the common "start one child and wait for it" case.
`AwaitTaskGroup` is the structured API for multiple children, result aggregation, `waitAny()`, or custom child
cancellation policy.

# Platform notes

`Await` inherits the platform shape of `Async`. POSIX backends can use `filePoll()` for ordinary file or pipe handles;
on Windows the awaiter fails fast instead of hanging because normal `AsyncFilePoll` support is not currently exposed for
those handles.

Thread-pool-backed file and filesystem awaiters use `AsyncFileRead`, `AsyncFileWrite`, `AsyncFileSend`, or
`AsyncFileSystemOperation` with caller-provided `ThreadPool` storage. Cancellation is still cooperative: when the work is
already running on a worker thread, completion may arrive before the stop request can take effect.

Local validation for Await changes should run macOS first, then Linux, then Windows. For targeted changes, prefer the
smallest relevant `SCAwaitTest` section when allocator behavior changes. Windows changes
should include focused `SCAwaitTest` Debug and Release runs before committing when the touched awaiter has
platform-specific behavior.

# Serial descriptors

`Await` does not currently add separate serial-port awaiter names. `SerialDescriptor` inherits from `FileDescriptor`, and
the lower-level `Async` tests exercise serial I/O through `AsyncFileRead` and `AsyncFileWrite`; the coroutine layer keeps
that shape by using `fileRead()` / `fileWrite()` with a `SerialDescriptor` after it has been associated with the
underlying `AsyncEventLoop`.

This keeps the surface area small and preserves the plain-`Result` rule: if a future dedicated serial helper is added, it
should still return `Result` and write any extra output into an explicit caller-owned result object.

# Stability Criteria

Before `Await` should move from MVP / Experimental to a stable status, the remaining design forks should be resolved:

- lifecycle hardening: ASan-covered teardown tests for thread-pool-backed operations and child-task destruction;
- optional helper APIs: whether filesystem watcher stream adapters are useful enough to add as explicit caller-owned
  `Await*` objects.

No-stdlib coroutine support is not required for the current MVP status. `Await` can remain isolated in `SCAwaitTest`
while the API is still moving; a `<coroutine>` replacement and normal `SCTest` participation are stable-track work.

# Cancellation

Cancellation is cooperative and follows the active awaiter's `AsyncRequest::stop()` when one exists. Cancelled awaits
return `AwaitCancelledResult()`, and callers can use `AwaitIsCancelled(result)` when cancellation needs to be
distinguished from ordinary failure while still preserving the plain `Result` API.

`AwaitTask::cancel()` is idempotent after cancellation has been requested. Cancellation is best-effort: if the suspended
operation is still active, `Await` asks the underlying `AsyncRequest` to stop and resumes the coroutine with the
cancellation result. If the operation has already completed, the normal completion result wins. Cancelling an already
completed task succeeds and leaves its result unchanged. Awaiter callbacks resume their continuation synchronously, so
there is no public "completed but not yet observed" cancellation window; tests cover the observable completion-wins
behavior instead.

# Lifetime rules

Awaiters keep the same stable-object rules as `Async`: sockets, file descriptors, buffers, result objects, wake-up
objects, and child tasks must stay alive while the operation is active.

When a completed child task is destroyed while an `Async` callback is still unwinding, `AwaitEventLoop` defers the
actual coroutine frame destruction until `run()`, `runOnce()`, or `runNoWait()` returns. This keeps any embedded
`AsyncRequest` alive until the lower-level event loop has finished its teardown work, without adding dynamic allocation.

Result objects may contain spans into caller-provided buffers. For example, `AwaitSocketReceiveResult::data` and
`AwaitFileReadResult::data` point into the receive/read buffer passed to the awaiter. Keep those buffers alive until
after the result has been inspected or copied elsewhere.

# Task groups

`AwaitTaskGroup` stores caller-owned task pointers in caller-provided storage. Its default cancellation policy is
structured: cancelling the parent task while it is suspended in `waitAll()` or `waitAny()` cancels active children before
the parent completes. `AwaitTaskGroupCancelPolicy::LeaveChildrenRunning` exists for advanced cases where child tasks
outlive the waiting parent.

For request-backed children, keep the child `AwaitTask` objects in caller-owned storage that outlives the parent
coroutine suspension. This mirrors `Async`'s stable request-object rule: the coroutine frame owns the active awaiter,
so the task object should not be a short-lived temporary when the event loop may still be unwinding an async
completion. `Examples/AwaitTaskGroupFiles` keeps each child task inside a caller-owned job object.

`waitAny()` defaults to `AwaitTaskGroupWaitAnyPolicy::CancelRemaining`, so stack-owned child tasks are not left active
after the first child completes. Use `LeaveRemainingRunning` only when pending children have an explicitly managed
lifetime.

After `waitAll()` returns, `collectResults()` can copy each child `Result` into caller-provided storage and optionally
fill `AwaitTaskGroupResultSummary` with counts plus the first failed task. This keeps aggregation no-allocation and
still follows Sane C++'s plain-`Result` style.

# Detached tasks

The preferred model remains structured: keep child `AwaitTask` objects in caller-owned storage and wait through
`AwaitTaskGroup` or `spawnAndWait()`. When a workflow really needs detached/background ownership,
`AwaitTaskRegistry` provides the no-allocation shape: fixed caller-provided task slots, explicit `spawn()` into the
first free slot, `waitAll()` to drain currently registered tasks, `waitAny()` to wait for the first completed slot,
`cancelAll()` during shutdown, and `clearCompleted()` for cleanup and optional result aggregation.

Like `AwaitTaskGroup::waitAny()`, registry `waitAny()` defaults to cancelling the remaining active tasks after the
winner is known. Pass `AwaitTaskRegistryWaitAnyPolicy::LeaveRemainingRunning` only when the registry storage is a true
background lifetime owner and shutdown will later call `cancelAll()` or drain the active tasks.

`AwaitTaskRegistry` owns only task bookkeeping. It does not allocate, create coroutine frames, or hide lifetime
management. Coroutine frames come from the `AwaitAllocator` bound to the task's `AwaitEventLoop`, and active registry
tasks must be cancelled or drained before the registry storage is destroyed.

Recommended shutdown for a registry is explicit and repeatable:

```cpp
SC_TRY(registry.cancelAll());
SC_TRY(await.run());

AwaitTaskGroupResultSummary summary;
registry.clearCompleted(&summary);
```

Calling `cancelAll()` on an empty or already-cancelled registry is valid. Calling `clearCompleted()` repeatedly is also
valid; it only releases completed slots. If a registry uses `waitAny(LeaveRemainingRunning)`, the owner must still call
`cancelAll()` or otherwise drain the remaining active tasks before destroying the registry storage.

# Memory allocation

`AwaitAllocator` is mandatory: every `AwaitEventLoop` receives an already opened allocator, and coroutine frame
allocation fails cleanly if no `AwaitEventLoop&` can be discovered from the coroutine parameters. There is no hidden
standard allocation fallback.

The default and recommended mode is fixed caller-owned storage through `createFixed(Span<char>)`. This preserves the
Sane C++ no-hidden-allocation rule while still allowing coroutine frames to be allocated and released individually.

Explicit opt-in modes exist for integration and experiments:

- `createVirtual()` reserves virtual address space and commits pages lazily until `close()`;
- `createMalloc()` uses one `malloc()` / `free()` pair per allocation;
- `createPolymorphic()` delegates to a caller-provided `AwaitAllocatorInterface` without depending on the Memory
  library.

All modes expose the same diagnostics through `AwaitAllocatorStatistics`: allocation/release counts, requested
allocated/released bytes, bytes in use, peak bytes in use, failed allocation count, and failed allocation sizes.

When sizing fixed storage, start from the smallest realistic workflow, run it once, inspect `peakUsed()` or
`statistics().peakBytesInUse`, then add headroom for the maximum number of concurrently active coroutine frames. A task
that has completed but whose `AwaitTask` object is still alive may keep its frame allocated until that task object is
destroyed or cleared from a registry.

```cpp
char           allocatorStorage[16 * 1024] = {};
AwaitAllocator allocator;
SC_TRY(allocator.createFixed(allocatorStorage));

AwaitEventLoop await(async, allocator);

// Run the workflow...

const AwaitAllocatorStatistics stats = allocator.statistics();
if (stats.numAllocationFailures != 0)
{
    return Result::Error("Await allocator storage is too small");
}
```

Fixed storage should be sized intentionally per workflow. Prefer using diagnostics to tune the storage over switching to
`createMalloc()` or `createVirtual()` just to hide a missing sizing decision.

# Exceptions

`Await` does not use exceptions for control flow. The C++20 test and examples intentionally keep
`CompileFlags::enableExceptions` disabled, so macOS, Linux, and Windows validation covers the exception-disabled path.
`AwaitTask::Promise::unhandled_exception()` remains present because the C++ coroutine promise interface requires it when
compiling against the standard coroutine header.

# Strict no-stdlib coroutine status

The strict `SC_INCLUDE_STD_CPP=0` coroutine story is intentionally not part of the current MVP bar. Today
`Libraries/Await/Internal/AwaitCoroutine.h` includes `<coroutine>` for `std::coroutine_traits`,
`std::coroutine_handle`, and `std::suspend_always`. Coroutine frame storage is still provided by `AwaitAllocator`;
there is no `std::nothrow` allocation fallback.

A future `-nostdinc++` shim looks technically possible but should be treated as Stable-track work. It would need to
provide the compiler-facing coroutine names expected by C++20 and map handles to the compiler coroutine builtins on
each supported compiler. Until that is proven on macOS, Linux, and Windows, `Await` stays in `SCAwaitTest` instead of
the normal `SCTest` path.

# Roadmap

🟨 MVP Follow-ups:

- Add only the next `Async` operations that have concrete examples or migration pressure.
- Add ASan-focused teardown coverage for thread-pool-backed awaiters and child-task destruction.
- Decide whether filesystem watcher integration should become an explicit stream/channel-style adapter.
- Continue tightening coroutine allocation portability, especially around member coroutine allocation discovery.
- Prototype and validate the no-stdlib coroutine shim behind an opt-in macro before moving `Await` into `SCTest`.

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 730			| 253		| 983	|
| Sources   | 1643			| 333		| 1976	|
| Sum       | 2373			| 586		| 2959	|
