@page library_fibers_async FibersAsync

@brief 🟥 Stackful fiber I/O bridge over Async

[TOC]

[SaneCppFibersAsync.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFibersAsync.h)
lets a [Fibers](@ref library_fibers) task wait for [Async](@ref library_async) I/O using ordinary function calls. It is
for code that benefits from blocking-shaped control flow but must not block the event-loop thread.

# Dependencies
- Dependencies: [Async](@ref library_async), [Fibers](@ref library_fibers)
- All dependencies: [Async](@ref library_async), [Fibers](@ref library_fibers), [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](FibersAsync.svg)


# The bridge, not another runtime

`FibersAsync` does not own an I/O runtime or a scheduler. Its central object, SC::FiberAsyncIO, joins a caller-owned
SC::AsyncEventLoop to a caller-owned SC::FiberScheduler. A fiber calls `sleep`, `receive`, `fileRead`, or another helper;
the bridge starts the corresponding `AsyncRequest`, suspends that fiber, and makes it ready again when the request
completes. Other fibers and callbacks can continue on the same loop in the meantime.

This is the useful mental model:

1. operation state and the underlying async request live on the current fiber's stable stack;
2. the fiber waits on a `FiberCounter`, yielding its OS thread;
3. the async callback records a plain SC::Result and wakes the fiber;
4. the original call resumes and returns that result.

There is no coroutine transformation and no second hidden event loop. The price is that every call must run inside a
scheduled fiber, and the application must explicitly drive both the scheduler and the event loop.

# A complete wait

The demo's sleep example is compiled as part of the `FibersDemo` executable:

@snippet Examples/FibersDemo/FibersDemo.cpp FibersAsyncSleepSnippet

SC::FiberAsyncIO::runUntilComplete drives ready fibers and async completions until neither remains. The shorter
`runOnce`, `runNoWait`, and `runUntilIdle` variants exist for embedding the bridge in a larger application loop. A
successful wait does not mean that an OS thread slept: only the calling fiber was suspended.

The example also exposes the storage model. The caller supplies the event loop, scheduler, command slots, tasks, stacks,
task group, and error collection. `FiberAsyncIO` itself borrows the event loop, scheduler, and command span; the
scheduler and task group use the remaining storage.

# What the I/O calls return

Operations return SC::Result for success, cancellation, or an OS/backend error. Values produced by an operation use
small explicit output records:

- socket sends report a byte count; receives report a span into the supplied buffer and whether the peer disconnected;
- file reads report a span into the supplied buffer and EOF; writes report the byte count;
- file-to-socket transfer reports bytes transferred and whether the backend used zero-copy;
- process and signal waits report exit or delivery information.

One-shot calls such as `send` and `fileWrite` may complete only part of the input. Use `sendAll` or `fileWriteAll` when
the whole span is required. Likewise, `fileReadExact` repeats reads until the buffer is full or EOF is reached. Offset
variants keep the position explicit rather than advancing the descriptor's shared position.

The bridge currently covers timers, TCP and datagram sockets, files and readiness, file-to-socket transfer, process
exit, and one-shot signal waits. It is not a stream protocol, buffered reader, DNS resolver, or general blocking-call
adapter; framing, buffering, retries, and higher-level protocols remain application or neighboring-library policy.

# Memory and lifetime are part of the API

Normal operation does not allocate a coroutine frame or heap-backed request. That predictability requires several hard
lifetime rules:

- SC::AsyncEventLoop and SC::FiberScheduler must outlive SC::FiberAsyncIO;
- each SC::FiberTask and its SC::FiberStack must remain alive and memory-stable until the task completes;
- descriptors, input spans, receive buffers, and output records must remain valid until their call resumes;
- cross-thread SC::FiberAsyncCommand storage must outlive the bridge and any operation using it.

The request and callback state are stack-local, which is safe specifically because a suspended fiber retains its stack.
Destroying or moving any referenced storage early is a correctness bug, not an operation the bridge copies around.

Capacity is similarly explicit. The application chooses the number and size of fiber stacks and, for worker-pool I/O,
the number of command slots. This makes peak memory visible, but it also means exhaustion is possible and must be
designed for.

# Worker threads and the event-loop owner

SC::AsyncEventLoop remains owner-thread-affine. A fiber may nevertheless resume on a worker thread. In that case the
bridge queues the request's start or stop procedure in caller-provided SC::FiberAsyncCommand storage, wakes the owner,
and waits for the owner to execute it.

The source-backed worker-pool example shows the division of responsibilities:

@snippet Examples/FibersDemo/FibersDemo.cpp FibersAsyncWorkerPoolSnippet

Only the owner thread calls `runOwnerUntilComplete`; worker threads execute fibers. Command storage is a bounded queue,
not scratch space sized only for one call. If simultaneous cross-thread starts and cancellation stops can fill it, the
operation fails rather than allocating. Size it from the intended concurrency or impose producer backpressure. For a
single-threaded scheduler, command storage can be omitted.

All `run*`, `runOwner*`, and `cancelAll()` methods are event-loop-owner calls. Every I/O operation method, including a
zero-length fast path, must be called by a currently running fiber of the bridge's scheduler. The bridge rejects an
operation before creating pending state or starting an `AsyncRequest` when that fiber context is absent; this keeps its
stack-local request and callback state valid for the entire suspension.

# Cancellation and shutdown

Cancellation is cooperative. SC::FiberScheduler::requestCancel wakes an interruptible fiber wait; the bridge stops the
underlying request when necessary and resumes the call with an error `Result`. SC::FiberAsyncIO::cancelAll asks the
scheduler to cancel all active fibers. Their pending bridge operations are then stopped through the same cooperative
path; callback-style requests sharing the event loop are not selected by this call.

Cancellation does not relax lifetime rules. The owner loop still has to process the stop/completion path, the fiber must
resume, and its task, stack, buffers, and request dependencies must remain alive until that happens. Treat
`runUntilComplete`/`runOwnerUntilComplete`, task result collection, and `AsyncEventLoop::close` as an ordered shutdown
sequence.

`FiberAsyncIO` is a non-copyable, non-movable borrowed wrapper. The scheduler, event loop, command storage, operation
buffers, and descriptors must outlive every operation that uses them. Destroying the wrapper with a pending operation
or queued owner-thread command is a programming error diagnosed by a release assertion. Command exhaustion is a normal
bounded-capacity error: it does not consume a slot permanently, and the same wrapper remains usable after the owner
drains previously queued work.

# Choosing between the neighboring libraries

Use [Async](@ref library_async) directly when callbacks fit the state machine, minimizing per-operation stack memory and
making every suspension point explicit. Add `FibersAsync` when a workflow is easier to express as nested ordinary calls
or must suspend through existing call layers. Each concurrent fiber then needs its own explicitly sized stack.

[Await](@ref library_await) offers another sequential syntax over the same `AsyncEventLoop`. It uses C++20 `co_await`
and allocator-backed coroutine frames; `FibersAsync` uses stackful switching and caller-provided stacks. Await generally
fits code designed around coroutines. FibersAsync fits code that needs to suspend through ordinary non-coroutine helper
functions, at the cost of coarser stack reservations and a draft runtime surface.

[Fibers](@ref library_fibers) remains the scheduling and synchronization layer. It is useful without I/O, while
`FibersAsync` is specifically the adapter from its tasks to `Async`. Neither library turns arbitrary blocking system
calls into cooperative waits.

# Status and practical limits

🟥 Draft

The implementation has focused tests for same-thread and worker-pool waits, sockets, datagrams, files, `fileSend`,
process exit, signals, cancellation races, and command-queue overflow on macOS, Linux, and Windows. The API is still
draft: DNS and stream conveniences are absent, backend coverage follows what `Async` supports on each platform, and the
run-loop/cancellation surface may evolve as larger applications exercise it.

For the exact operation list and result fields, see [SC::FiberAsyncIO](@ref SC::FiberAsyncIO) and the
[FibersAsync module](@ref group_fibers_async). `Examples/FibersDemo` is the smallest end-to-end example;
`Tests/Libraries/FibersAsync/FibersAsyncTest.cpp` contains the edge cases.

# Roadmap

- Add DNS or stream helpers only when concrete consumers establish the right ownership and buffering model.
- Continue stress-testing cancellation, shutdown, and command-capacity races.
- Consider a backend-neutral fiber I/O facade only after a second real backend exists.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/FibersAsync`.
Single File counts `SaneCppFibersAsync.h`.
Standalone counts `SaneCppFibersAsyncStandalone.h` and intentionally includes dependency payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 147    | 910    | 1057  |
| Single File | 510    | 1063   | 1573  |
| Standalone  | 8416   | 19290  | 27706 |
