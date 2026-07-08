@page library_fibers_async FibersAsync

@brief 🟥 Stackful fiber I/O bridge over Async

[TOC]

[SaneCppFibersAsync.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFibersAsync.h) is a bridge between [Fibers](@ref library_fibers) and [Async](@ref library_async). It lets fiber tasks call async I/O helpers in synchronous-looking code while preserving the caller-owned request, buffer, and event-loop model from `Async`.


# Dependencies
- Dependencies: [Async](@ref library_async), [Fibers](@ref library_fibers)
- All dependencies: [Async](@ref library_async), [Fibers](@ref library_fibers), [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](FibersAsync.svg)


# What FibersAsync Is For

`FibersAsync` is the I/O bridge for stackful fibers. It does not replace `AsyncEventLoop` and it does not replace
`FiberScheduler`. Instead, `FiberAsyncIO` wraps both:

```cpp
AsyncEventLoop eventLoop;
SC_TRY(eventLoop.create());

FiberScheduler    scheduler;
FiberAsyncCommand commands[8];
FiberAsyncIO      io(scheduler, eventLoop, commands);
```

Fiber tasks can then wait for timers, sockets, files, processes, or signals by calling `io.sleep()`, `io.receive()`,
`io.fileRead()`, and similar methods. Internally, the lower-level `AsyncRequest` starts on the wrapped event loop, and
the current fiber suspends until the async completion wakes it.

# Design Intent

The library keeps the spirit of `Async`:

- `FiberAsyncIO` uses an externally owned `AsyncEventLoop&`;
- `FiberAsyncIO` uses an externally owned `FiberScheduler&`;
- methods return plain `Result`;
- extra operation outputs are explicit caller-provided result objects;
- buffers, sockets, files, tasks, stacks, and output objects must remain valid while the operation is active;
- callback-style `Async` code and fiber-style `FibersAsync` code can share the same event loop;
- cross-thread event-loop access goes through bounded caller-provided `FiberAsyncCommand` storage.

`FibersAsync` is deliberately concrete for now. A more abstract `FiberIO` facade should wait until there is a second real
backend worth sharing behind one API.

# A Sleep Example

The simplest example is a fiber that waits without blocking the OS thread:

```cpp
struct State
{
    FiberAsyncIO* io        = nullptr;
    int           completed = 0;
};

AsyncEventLoop eventLoop;
SC_TRY(eventLoop.create());

FiberScheduler scheduler;
FiberAsyncIO   io(scheduler, eventLoop);
FiberTask      task;
char           stackMemory[64 * 1024] = {};
FiberStack     stack({stackMemory, sizeof(stackMemory)});
State          state;
state.io = &io;

SC_TRY(scheduler.spawn(task, stack,
                       FiberTask::Procedure(
                           [&state](FiberScheduler&)
                           {
                               SC_TRY(state.io->sleep(TimeMs{1}));
                               state.completed++;
                               return Result(true);
                           })));

SC_TRY(io.runUntilComplete());
SC_TRY(task.result());
SC_TRY(eventLoop.close());
```

`io.runUntilComplete()` drives both sides: ready fibers through `FiberScheduler`, and async completions through
`AsyncEventLoop`.

# Socket Echo Shape

Socket helpers follow the same Sane C++ result-object pattern as `Async`: operation status is returned as `Result`, and
data about the operation is written into explicit output objects.

```cpp
Result echoOnce(FiberAsyncIO& io, const SocketDescriptor& socket)
{
    char receiveBuffer[1024] = {};

    FiberAsyncSocketReceiveResult received;
    SC_TRY(io.receive(socket, {receiveBuffer, sizeof(receiveBuffer)}, received));

    if (received.disconnected)
    {
        return Result(true);
    }

    FiberAsyncSocketSendResult sent;
    SC_TRY(io.sendAll(socket, received.data, &sent));
    return Result(true);
}
```

`receive()` writes the actual received byte range into `received.data`, which points into the caller-provided buffer.
`sendAll()` repeats lower-level send operations until the whole span is sent or an error/cancellation occurs.

# File I/O Shape

File helpers also use caller-provided buffers and optional result objects:

```cpp
Result copyChunk(FiberAsyncIO& io, const FileDescriptor& input, const FileDescriptor& output)
{
    char readBuffer[4096] = {};

    FiberAsyncFileReadResult readResult;
    SC_TRY(io.fileRead(input, {readBuffer, sizeof(readBuffer)}, readResult));
    if (readResult.endOfFile or readResult.data.sizeInBytes() == 0)
    {
        return Result(true);
    }

    FiberAsyncFileWriteResult writeResult;
    SC_TRY(io.fileWriteAll(output, readResult.data, &writeResult));
    return Result(true);
}
```

Offsets are explicit through `fileReadAt()`, `fileReadExactAt()`, `fileWriteAt()`, and `fileWriteAllAt()`. File
readiness is available through `filePoll()` for the currently supported platform/backend cases.

# Worker-Pool I/O

`AsyncEventLoop` remains owner-thread-affine. If a fiber running on a worker thread starts I/O, `FiberAsyncIO` posts the
start/stop command back to the owner thread through bounded `FiberAsyncCommand` storage.

```cpp
static constexpr size_t NumWorkers = 2;

AsyncEventLoop eventLoop;
SC_TRY(eventLoop.create());

FiberScheduler    scheduler;
FiberAsyncCommand commands[8];
FiberAsyncIO      io(scheduler, eventLoop, commands);

FiberWorker       workers[NumWorkers];
FiberWorkerThread threads[NumWorkers];
FiberWorkerPool   workerPool;

SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
SC_TRY(io.runOwnerUntilComplete());
SC_TRY(workerPool.join());
SC_TRY(eventLoop.close());
```

The command storage size is a real capacity limit. If cross-thread producers can submit more simultaneous starts/stops
than the storage can hold, provide more `FiberAsyncCommand` slots or design the producer to apply backpressure.

# Cancellation

Cancellation is cooperative and result-based. A task suspended in a `FiberAsyncIO` operation can be canceled through the
fiber scheduler; `FiberAsyncIO` stops the underlying async request when needed and wakes the fiber with an error
`Result`.

```cpp
SC_TRY(scheduler.requestCancel(task));
SC_TRY(io.runUntilComplete());

if (not task.result())
{
    // The task observed cancellation or another operation error.
}
```

`FiberAsyncIO::cancelAll()` is also available as an I/O bridge helper for canceling pending async operations associated
with the bridge.

# Lifetime Rules

The lifetime rules are intentionally close to `Async` and `Fibers`:

- `AsyncEventLoop` must outlive `FiberAsyncIO`;
- `FiberScheduler` must outlive `FiberAsyncIO`;
- `FiberAsyncCommand` storage must outlive cross-thread operations using it;
- `FiberTask` and `FiberStack` storage must outlive the spawned task;
- sockets, files, buffers, and output result objects must outlive the operation using them;
- request objects used internally are stack-local to the suspended fiber and remain valid because the fiber stack is
  stable while suspended.

This is the key difference from a heap-backed async framework: the caller chooses the maximum number of simultaneous
tasks, stacks, command posts, and buffers.

# Allocation Model

`FibersAsync` does not allocate coroutine frames and does not need C++20 coroutines. It relies on the active fiber stack
to hold the operation state while the fiber is suspended. Cross-thread command posting uses caller-provided
`Span<FiberAsyncCommand>` storage, and normal same-thread usage can omit command storage.

The lower-level `Async` and `Fibers` rules still apply: request objects and fiber objects must be memory-stable, and any
capacity that can grow must be supplied explicitly by the caller.

# Relationship To Await

[Await](@ref library_await) and `FibersAsync` both make `Async` code easier to read, but they make different tradeoffs.

`Await` uses C++20 coroutines and explicit `AwaitAllocator` coroutine-frame allocation. It is a good fit when you want
`co_await` syntax and compiler-generated coroutine state machines.

`FibersAsync` uses stackful fibers. It is a good fit when you want ordinary synchronous-looking function calls, the
ability to suspend through existing nested call stacks, and explicit stack storage instead of coroutine frames.

Both libraries keep callback-style `Async` integration possible because both wrap an existing `AsyncEventLoop&` instead
of owning a separate I/O runtime.

# Features

| FibersAsync API                                      | Description                                      |
|:----------------------------------------------------|:-------------------------------------------------|
| [FiberAsyncIO](@ref SC::FiberAsyncIO)               | Synchronous-looking fiber I/O wrapper around an externally owned `AsyncEventLoop`. |
| [FiberAsyncCommand](@ref SC::FiberAsyncCommand)     | Bounded command slot for owner-thread I/O posting. |
| [FiberAsyncSocketSendResult](@ref SC::FiberAsyncSocketSendResult) | Result object populated by `send`, `sendTo`, and `sendAll`. |
| [FiberAsyncSocketReceiveResult](@ref SC::FiberAsyncSocketReceiveResult) | Result object populated by `receive`. |
| [FiberAsyncSocketReceiveFromResult](@ref SC::FiberAsyncSocketReceiveFromResult) | Result object populated by `receiveFrom`. |
| [FiberAsyncFileWriteResult](@ref SC::FiberAsyncFileWriteResult) | Result object populated by file write helpers. |
| [FiberAsyncFileReadResult](@ref SC::FiberAsyncFileReadResult) | Result object populated by file read helpers. |
| [FiberAsyncFileSendOptions](@ref SC::FiberAsyncFileSendOptions) | Options for `fileSend`. |
| [FiberAsyncFileSendResult](@ref SC::FiberAsyncFileSendResult) | Result object populated by `fileSend`. |
| [FiberAsyncProcessExitResult](@ref SC::FiberAsyncProcessExitResult) | Result object populated by `processExit`. |
| [FiberAsyncSignalResult](@ref SC::FiberAsyncSignalResult) | Result object populated by `signal`. |

# Complete Examples

- `Examples/FibersDemo` shows two sleeping fiber tasks driven by one `FiberAsyncIO`, then the same bridge used with a
  worker pool.
- `Tests/Libraries/FibersAsync/FibersAsyncTest.cpp` contains focused examples for sleeps, sockets, UDP, files,
  `fileSend`, process exit, signals, cancellation, command queue overflow, and worker-pool cross-thread operation
  posting.
- `Examples/FibersBenchmark` focuses on CPU scheduling rather than I/O, but it is useful context for how the same
  scheduler scales task execution.

# Status

🟥 Draft

Current support includes:

- `sleep()`;
- socket `accept()` and `connect()`;
- socket `receive()`;
- one-shot socket `send()`;
- socket `sendAll()`;
- datagram socket `sendTo()` and `receiveFrom()`;
- file `fileRead()`, `fileReadAt()`, `fileReadExact()`, `fileReadExactAt()`;
- file `fileWrite()`, `fileWriteAt()`, `fileWriteAll()`, and `fileWriteAllAt()`;
- file readiness through `filePoll()` where supported by the underlying `Async` backend;
- file-to-socket transfer through `fileSend()`;
- process exit waiting through `processExit()`;
- one-shot signal waiting through `signal()`;
- owner-thread diagnostics through release assertions and `isOwnerThread()`;
- bounded cross-thread command posting from worker fibers to the event-loop owner thread;
- cooperative cancellation of pending operations through `FiberScheduler::requestCancel*` and `FiberAsyncIO::cancelAll()`;
- focused macOS, Linux, and Windows test coverage in `FibersAsyncTest`.

# Roadmap

- Add DNS and stream helpers when real examples justify the API.
- Explore blocking/threaded backends only after the concrete `FiberAsyncIO` bridge remains stable.
- Keep expanding cancellation and shutdown stress tests around stop/complete races.
- Revisit integration adapters between `AwaitTask` and fiber tasks after both libraries stabilize.
- Consider a runtime-selectable `FiberIO` facade only after at least two real backends exist.

# Details

@copydetails group_fibers_async
