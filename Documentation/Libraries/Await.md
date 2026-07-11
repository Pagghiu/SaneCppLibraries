@page library_await Await

@brief 🟨 C++20 coroutine layer over Async

[TOC]

[SaneCppAwait.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppAwait.h) adds `co_await`
syntax to [Async](@ref library_async). It is for workflows where callback state machines obscure the operation being
performed, but where the explicit storage, lifetime, and error-handling rules of Sane C++ must remain visible.

@warning
This library is currently **MVP / Experimental**. It covers useful `Async` workflows, but the API may still change as
the coroutine model is refined. Use it for focused experiments and examples, not as a stable public API yet.

# Dependencies
- Dependencies: [Async](@ref library_async)
- All dependencies: [Async](@ref library_async), [File](@ref library_file), [FileSystem](@ref library_file_system), [Socket](@ref library_socket), [Threading](@ref library_threading)

![Dependency Graph](Await.svg)


# When Await Fits

Choose `Await` when a sequence of asynchronous operations is easier to understand as ordinary control flow, you can use
C++20 coroutines, and you want to keep using `AsyncEventLoop`. Coroutine functions return `AwaitTask`; awaited operations
and coroutine completion return plain `Result`; additional outputs go into explicit caller-owned result objects.

`Await` is not a new I/O backend and does not make asynchronous work blocking. `AwaitEventLoop` wraps an existing
caller-owned `AsyncEventLoop&`. While a coroutine is suspended, that underlying loop still performs the work and resumes
the coroutine from the completion callback. Callback-style `Async` requests and coroutine-style tasks can therefore
share one loop during an incremental migration.

Consider another layer when:

- the API must already be stable;
- the project cannot use C++20 coroutines;
- callbacks already express the workflow clearly—in that case [Async](@ref library_async) is the smaller layer;
- stackful synchronous-looking code is preferred and fixed task stacks are acceptable—in that case
  [FibersAsync](@ref library_fibers_async) makes a different tradeoff.

# The Programming Model

The compiler generates the coroutine state machine, but does not take ownership decisions away from the program:

- `AwaitEventLoop` borrows an `AsyncEventLoop` and an opened `AwaitAllocator`;
- every coroutine frame is allocated explicitly through that allocator;
- sockets, file descriptors, buffers, output objects, wake-up objects, and child task objects remain stable while active;
- `SC_CO_TRY(co_await operation)` propagates a failed `Result` with `co_return`;
- the owner drives progress with `run()`, `runOnce()`, or `runNoWait()` and explicitly drains or cancels work at shutdown.

The following compiled test illustrates coroutine composition: two child tasks share the loop and caller-owned sockets,
keep suspended state in their explicit coroutine frames, and are joined by a task group with fixed pointer storage. The
client's reply buffer and result object remain owned by the calling scope. The surrounding test supplies the allocator,
constructs and drives the event loop, and checks the root task's result.

@snippet Tests/Libraries/Await/AwaitTest.cpp AwaitEchoConversationSnippet

The result spans produced by receive and read operations point into the supplied buffers. Inspect or copy those results
before the buffers go out of scope.

# Coroutine Frame Allocation

`AwaitAllocator` is mandatory; there is no hidden standard allocation fallback. The recommended mode is
`createFixed(Span<char>)`, which allocates and releases individual coroutine frames inside caller-provided storage.
`AwaitAllocatorStatistics` reports current and peak use, the largest successful request, and allocation failures, so a
workflow can be measured and its arena sized with deliberate concurrency headroom.

Three explicit opt-in modes exist for integration work:

- `createVirtual()` reserves virtual address space and commits pages lazily;
- `createMalloc()` performs a `malloc()` / `free()` pair per allocation;
- `createPolymorphic()` delegates to a caller-provided `AwaitAllocatorInterface`.

These modes are visible policy choices, not transparent fallbacks. A completed task can retain its frame until its
`AwaitTask` is destroyed or cleared from a registry, so arena sizing must account for both active concurrency and
completed-but-owned tasks. Current examples use fixed arenas in the 8–20 KiB range and expose allocator diagnostics;
those numbers are examples, not portable guarantees about coroutine frame size.

# Structured and Background Tasks

For one child, `spawnAndWait()` starts and joins an existing `AwaitTask`. For several related children,
`AwaitTaskGroup` stores task pointers in caller-provided storage and offers `spawn()`, `spawnAll()`, `waitAll()`, and
`waitAny()`. Its structured defaults cancel active children when a waiting parent is cancelled and cancel remaining
siblings after `waitAny()` finds a winner.

Escape-hatch policies (`LeaveChildrenRunning` and `LeaveRemainingRunning`) are available only for owners that can prove
the child storage outlives the waiter. Result aggregation is likewise bounded: `collectResults()` writes into supplied
storage, while `summarizeResults()` returns counts and the first failure without allocating.

`AwaitTaskRegistry` handles genuinely detached or background tasks with fixed caller-provided slots. The registry owns
bookkeeping, not coroutine frames. Shutdown remains explicit: cancel or drain active tasks, run the loop so cancellation
can complete, then clear completed slots. There is intentionally no destructor that hides a fallible cancel-and-drain
sequence.

# Cancellation and Lifetime

Cancellation is cooperative. When possible, the active awaiter asks its underlying `AsyncRequest` to stop and resumes
with `AwaitCancelledResult()`; `AwaitIsCancelled()` distinguishes that result from ordinary failure. If completion has
already won the race, the normal completion result remains. Cancelling a completed task succeeds without replacing its
result.

Destroying an active `AwaitTask` is an assert-release programming error. During callback unwinding, destruction of a
completed child frame is deferred until the event-loop run call returns, keeping embedded `AsyncRequest` state alive
through lower-level teardown without allocating.

Thread-pool-backed file and filesystem operations are also cooperative: once work is executing on a worker, completion
may arrive before a stop request takes effect. This is an important distinction from preemptive cancellation.

# I/O Shape

Awaiters fall into two useful categories.

Direct wrappers preserve one lower-level operation: `send()` may send only part of a stream buffer, `receive()` may
return any non-empty prefix, and `fileRead()` may read fewer bytes than requested. They are appropriate when the caller
wants to own the loop or protocol policy.

Bounded convenience helpers repeat an operation without introducing hidden storage: `sendAll()` sends the complete
caller span, `receiveExact()` fills a buffer or reports a partial result on disconnect, `receiveLine()` finds `\n` in a
fixed buffer and trims a preceding `\r`, and `fileReadUntilFullOrEOF()` fills a supplied buffer or stops at EOF.
`fileWrite()` already writes its supplied single or scatter/gather buffers fully. For portable offset-write examples,
prefer one contiguous buffer; scatter/gather writes combined with explicit offsets remain backend-sensitive.

The current surface covers timers; TCP and UDP sockets; loop wake-ups; files and POSIX file readiness; selected
thread-pool filesystem operations; process exit and one-shot signals; thread-pool work; child tasks, groups, registries,
and timeouts. `SerialDescriptor` can use the file awaiters because `Async` models serial I/O as file reads and writes.

Filesystem watching is deliberately not another one-shot `AwaitEventLoop` method. A watcher produces a long-lived event
stream, so a future coroutine integration is likely to need an explicit caller-owned adapter with bounded event storage
and overflow semantics—closer to a channel than to the request-shaped awaiters currently in this library.

# Examples

The examples under `Examples/` are small complete programs rather than isolated API demonstrations:

- `AwaitEcho`, `AwaitDatagramPing`, and `AwaitLineProtocol` cover stream and datagram protocols;
- `AwaitTaskGroupFiles`, `AwaitFirstResponse`, `AwaitDeadline`, and `AwaitBackgroundJobs` cover task ownership,
  fan-out, racing, timeouts, and detached work;
- `AwaitFilePatch`, `AwaitManifestPreview`, and `AwaitFileCourier` cover file I/O and file-to-socket transfer;
- `AwaitBackgroundDigest` and `AwaitProcessExitCodes` cover thread-pool work and process waiting;
- `AwaitCallbackBridge` demonstrates callback-style `Async` and coroutine-style `Await` sharing one loop;
- `AwaitConfigReload`, `AwaitThreadWakeUp`, and `AwaitServiceProbe` combine the primitives into application-shaped
  workflows and expose shutdown or allocator concerns.

# Platform and Build Boundaries

The default path uses the standard `<coroutine>` header with exceptions disabled. `SCAwaitTest` is separate from
`SCTest` because normal Await tests and examples enable C++20 and standard C++ headers. Await itself still has no Memory
library dependency because frame storage is supplied through `AwaitAllocator`.

A narrow strict-no-stdlib path exists behind `SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE=1`. Its compiler-facing coroutine shim
is tested by `SCAwaitCoroutineShimTest`, but normal examples do not use it yet. Full `SCTest` and single-file-library
participation remain stability work.

`Await` inherits backend differences from `Async`. In particular, `filePoll()` supports suitable POSIX handles; on
Windows it fails fast because ordinary `AsyncFileReadiness` is not exposed for those handles.

# API Reference

The complete type and method reference is in [the Await API group](@ref group_await). Start with
[AwaitEventLoop](@ref SC::AwaitEventLoop), [AwaitTask](@ref SC::AwaitTask),
[AwaitAllocator](@ref SC::AwaitAllocator), [AwaitTaskGroup](@ref SC::AwaitTaskGroup), and
[AwaitTaskRegistry](@ref SC::AwaitTaskRegistry).

# Status

🟨 MVP / Experimental

The implemented surface is broad enough for realistic examples, but stability still depends on lifecycle hardening,
especially ASan-covered teardown for thread-pool-backed operations and child-task destruction. Optional stream adapters
should be added only after concrete workflows establish their storage and overflow semantics.

# Blog

Some relevant blog posts are:

- [May 2026 Update](https://pagghiu.github.io/site/blog/2026-05-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟨 MVP Follow-ups:

- Add only the next `Async` operations that have concrete examples or migration pressure.
- Add ASan-focused teardown coverage for thread-pool-backed awaiters and child-task destruction.
- Decide whether filesystem watcher integration should become an explicit stream/channel-style adapter.
- Continue tightening coroutine allocation portability, especially around member coroutine allocation discovery.
- Grow the no-stdlib coroutine shim from isolated probe coverage toward full Await coverage before moving `Await` into
  `SCTest`.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Await`.
Single File counts
`SaneCppAwait.h`.
Standalone counts `SaneCppAwaitStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 1199		| 3231		| 4430	|
| Single File | 1580		| 3382		| 4962	|
| Standalone  | 7770		| 16661		| 24431	|
