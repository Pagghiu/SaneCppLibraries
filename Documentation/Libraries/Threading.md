@page library_threading Threading

@brief 🟩 Native threads, synchronization primitives, and a caller-owned task pool

[TOC]

[SaneCppThreading.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppThreading.h) provides
small cross-platform wrappers around native threads and synchronization primitives, plus a fixed-worker thread pool.
It is intended for code that wants explicit lifetime and storage control without the STL, exceptions, RTTI, or a
general-purpose job runtime.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Threading.svg)


# When Threading Fits

Use `Threading` when an application needs a few long-lived native threads, direct mutex/condition-variable style
synchronization, or a simple pool for blocking work. The library exposes familiar low-level pieces without introducing
an allocator or a scheduler policy of its own.

It is deliberately not a replacement for the C++ standard concurrency library. There are no RAII lock guards,
futures, timed waits, cancellation tokens, work stealing, or dynamically growing task queues. Those omissions keep the
surface small and ownership visible, but application code must build any higher-level policy it needs.

The public building blocks are:

- `Thread`, a single native thread with explicit `start()`, `join()`, or `detach()`;
- `Mutex` and `ConditionVariable`, direct blocking synchronization primitives;
- `RWLock`, `Semaphore`, `Barrier`, and `EventObject`, implemented from those primitives;
- `Atomic<int32_t>` and `Atomic<bool>`, with explicit memory-order operations;
- `ThreadPool`, a fixed set of native workers consuming caller-owned tasks from an intrusive FIFO.

# Threads Have Explicit Terminal Ownership

A `Thread` stores its callable inside the thread object and creates one native operating-system thread. The object is
not copyable or movable, so its address and callable storage remain stable while the thread runs. After a successful
`start()`, the owner must call either `join()` or `detach()` before destroying the `Thread`; the destructor asserts in
debug builds when that obligation is missed.

The callable uses `SC::Function`, whose capture storage has a fixed inline capacity. An oversized capturing lambda is
rejected rather than heap-allocated; capture a pointer to caller-owned context when the state does not fit, and keep
that context alive for the whole execution.

`join()` waits for completion and releases the native thread handle. `detach()` releases ownership of that handle but
does not wait for the callable to finish. The running entry point still refers to the `Thread` object and its stored
callable, so the object and everything referenced by its captures must outlive the actual detached execution. Reusing
or destroying that storage merely because `detach()` returned is unsafe; detached completion needs an external
protocol if the owner must know when reclamation is safe.

Thread creation can fail and returns `Result`. Although the library performs no heap allocation itself, creating a
native thread allocates operating-system resources and a platform stack. `Thread::Sleep()` and thread naming are thin
platform operations; `setThreadName()` must be called by the thread being named.

# Choosing A Synchronization Primitive

`Mutex` is the basic exclusive lock. `ConditionVariable::wait()` expects an already locked `Mutex`, releases it while
waiting, and returns with it locked again. Waiting code must check its predicate in a loop. The API intentionally does
not provide a lock guard, so every control path is responsible for unlocking.

This compiled test shows the complete mutex shape:

@snippet Tests/Libraries/Threading/ThreadingTest.cpp mutexSnippet

The higher-level primitives encode a small amount of policy:

- `RWLock` allows concurrent readers and gives waiting writers priority over new readers.
- `Semaphore` blocks at a zero count and wakes one waiter on `release()`; it has no timed or non-blocking acquire.
- `Barrier` is reusable across generations. Every generation needs exactly the configured number of arrivals; losing
  a participant leaves the others blocked.
- `EventObject` retains a signal and, by default, automatically consumes it in one waiter. Setting its public
  `autoReset` field to `false` retains the signaled state, but `signal()` still wakes only one existing waiter rather
  than broadcasting. It is therefore not a complete manual-reset event, and the type has no separate reset operation.

None of these operations accept cancellation or timeouts. If shutdown must interrupt a blocked thread, that protocol
has to be represented in shared state and signaled explicitly by the application.

# A Bounded-By-Ownership Thread Pool

`ThreadPool` creates a fixed number of detached native workers. It does not allocate task nodes: each
`ThreadPool::Task` supplied by the caller contains its callable and the intrusive links used by the FIFO. This makes
queueing allocation-free inside the library and naturally prevents the same task object from being queued twice or
used by two pools at once. Task callables have the same fixed `SC::Function` capture capacity as thread callables.

The following SCTest example is compiled on every supported test build:

@snippet Tests/Libraries/Threading/ThreadPoolTest.cpp threadPoolSnippet

The important constraint is address and lifetime, not just capacity. A queued task must stay at the same address until
it finishes or the pool releases it during destruction. Everything referenced by its callable must remain valid for
the same interval. Arrays, arenas, or other caller-owned stable storage are natural ways to bound the number of tasks.

`waitForTask()` waits for one task and `waitForAllTasks()` drains all queued and running work. `destroy()` has different
semantics: it removes tasks that have not started, asks workers to stop, and waits only for work already running. Call
`waitForAllTasks()` before `destroy()` when every accepted task must execute.

Declaration order matters for stack-owned pools and tasks. Because local objects are destroyed in reverse order,
declare task storage before the pool, or explicitly drain and destroy the pool before task storage leaves scope. The
pool destructor calls `destroy()`, but it cannot make already-invalid task addresses safe.

This pool is a good fit for coarse blocking jobs and for libraries such as [Async](@ref library_async) that need a
caller-supplied blocking backend. It is less suitable for very small jobs, dependency graphs, priorities, recursive
parallelism, or workloads that need work stealing.

# Atomics Are Intentionally Narrow

`Atomic` currently has specializations only for `int32_t` and `bool`. The integer specialization provides load/store,
exchange, fetch-add/subtract, compare-exchange, and increment/decrement; the Boolean specialization omits arithmetic.
Both accept the library's `memory_order` values and default to sequential consistency.

This is enough for counters, flags, and small coordination protocols, but it is not a general `std::atomic<T>`
substitute: there are no pointer, 64-bit, user-defined, wait/notify, or atomic-flag specializations. Prefer the default
ordering unless a protocol has been designed and tested with weaker ordering.

# Relationship To Neighboring Libraries

`Threading` sits below several higher-level SC libraries but depends on none of them:

- [Async](@ref library_async) multiplexes operating-system I/O and can use `ThreadPool` for operations that may block.
  Choose it when one thread should coordinate many I/O operations rather than dedicating a thread to each operation.
- [Await](@ref library_await) adds C++20 coroutine syntax over `Async`; it does not change the underlying request and
  thread-pool lifetime rules.
- [Fibers](@ref library_fibers) is a stackful task scheduler with explicit stacks, task groups, and work distribution.
  Choose it when tasks need to suspend through ordinary call stacks or participate in a scheduler rather than merely
  run to completion on a simple FIFO worker pool.
- [FibersAsync](@ref library_fibers_async) connects fiber suspension to the asynchronous I/O loop.

Direct `Threading` primitives remain appropriate at application boundaries, for coarse background ownership, and when
building a deliberately small synchronization protocol. Moving to a neighboring library trades that directness for a
richer execution model.

# Status And Further Material

🟩 Usable. The main native threading and synchronization primitives are implemented. The clearest current
limitation is the deliberately small set of `Atomic<T>` specializations; the pool also remains intentionally simple
rather than aiming to be a full job system.

Development background:

- [August 2025 Update](https://pagghiu.github.io/site/blog/2025-08-31-SaneCppLibrariesUpdate.html)
- [Ep.13 - Simple ThreadPool](https://www.youtube.com/watch?v=e48ruImESxI)

# Roadmap

🟦 Complete Features:
- Support more types in `Atomic<T>`.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Threading`.
Single File counts
`SaneCppThreading.h`.
Standalone counts `SaneCppThreadingStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 433		| 926		| 1359	|
| Single File | 1088		| 1102		| 2190	|
| Standalone  | 1088		| 1102		| 2190	|
