@page library_fibers Fibers

@brief 🟥 Stackful cooperative task runtime

[TOC]

[SaneCppFibers.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFibers.h) is an experimental
stackful task runtime: code can suspend from an ordinary nested call stack without blocking the OS thread that runs it.
Tasks, stacks, workers, and bounded queues remain explicit program-owned resources.

@warning
The library is a draft. The implementation has broad test coverage, including multi-worker execution, but its API and
operational experience are not yet mature enough to treat it as a stable general-purpose job system.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Fibers.svg)


# Where Fibers Fits

`Fibers` is a CPU/tasking runtime for code that wants synchronous-looking control flow without blocking an OS thread.
A fiber owns a stack, can call normal C++ functions, and can cooperatively suspend with `FiberScheduler::yield()` or by
waiting on fiber primitives. Later it can resume on the same worker or on another worker.

It is aimed at bounded micro-tasking workloads: many short jobs over time, reusable task and stack slots, optional work
stealing between worker threads, and memory budgets selected in advance by the caller. It is not an I/O library,
preemptive thread scheduler, or transparent replacement for `std::thread`.

Use `Fibers` when you want:

- stackful tasks that can suspend from ordinary call stacks;
- caller-owned task, stack, worker, and queue storage;
- cooperative synchronization primitives such as events, semaphores, mutexes, counters, and task groups;
- a runtime that can run on one thread or on a caller-provided worker pool when parallelism is useful;
- no dependency on [Async](@ref library_async), [Await](@ref library_await), or [Threading](@ref library_threading).

# The Scheduling Model

`FiberScheduler` owns the logical scheduling state, but not the storage of the things it schedules. A task is made from:

- a caller-owned `FiberTask`;
- a caller-owned `FiberStack`, or a slot acquired through `FiberTaskPool`;
- a `FiberTask::Procedure` returning plain `Result`;
- optional cancellation, counter, and user-data inputs through `FiberTaskSpawnOptions`.

The scheduler runs a ready task until it completes, explicitly yields, or waits on a fiber primitive. This is cooperative:
a task that neither returns nor suspends monopolizes its worker. When a task becomes runnable again, intrusive links in
`FiberTask` put it back on a ready queue without allocating.

`runOnce()` and the other scheduler-driving calls are useful for a single-threaded owner. `FiberWorkerPool` instead owns
OS threads while it is running and lets workers steal ready tasks. Parallel workers do not change the cooperative rule
inside each task, and they mean resumed code must be safe to run on a different OS thread.

# A Representative CPU Workload

`FiberTaskPool` is the ergonomic way to run many bounded tasks without manually pairing each task with a stack. The
pool does not grow: if all slots are active, producers can wait for capacity and try again.

@snippet Examples/FibersDemo/FibersDemo.cpp FibersCpuTasksSnippet

This is still ordinary C++ control flow. The call to `yield()` cooperatively gives another ready fiber a chance to run,
but no OS thread is blocked waiting for preemption.

# Capacity Is Part of Control Flow

When producing more work than the pool can hold at once, capacity pressure is explicit. From inside a fiber,
`waitForSpawnCapacity()` suspends cooperatively until at least one pool slot is available.

Capacity is observable through `hasAvailableTask()` and `availableCount()`. `waitForSpawnCapacity()` cooperatively
suspends a producing fiber until a slot becomes available; an external producer must instead drive or coordinate with
the scheduler. There is no unbounded overflow queue hidden behind `spawn()`.

# Adding Worker Threads

`FiberWorkerPool` runs one `FiberScheduler` on caller-provided OS thread storage. Workers can steal ready fibers from
each other, and optional allocator-backed worker deques avoid placing worker queue storage on the heap.

```cpp
static constexpr size_t NumWorkers = 4;

FiberScheduler scheduler;
FiberWorker    workers[NumWorkers];
FiberWorkerThread threads[NumWorkers];
FiberWorkerPool workerPool;

SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
SC_TRY(workerPool.join());
```

For higher-throughput scheduling, explicit deque storage can be provided through `FiberAllocator`. The fixed allocator
uses a caller buffer; the virtual allocator reserves a caller-selected address-space budget and commits pages on demand.

```cpp
char           allocatorStorage[64 * 1024] = {};
FiberAllocator allocator;
SC_TRY(allocator.createFixed({allocatorStorage, sizeof(allocatorStorage)}));

FiberWorkerPoolOptions options;
options.dequeAllocator         = &allocator;
options.dequeCapacityPerWorker = 256;
options.injectionAllocator     = &allocator;
options.injectionCapacity      = 256;

SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, options));
SC_TRY(workerPool.join());
SC_TRY(allocator.close());
```

The worker pool owns OS threads while running, but the memory for workers, thread handles, deques, tasks, and stacks is
still selected by the caller. `join()` waits for scheduled work and worker shutdown; lifecycle ordering is therefore
part of application shutdown rather than destructor magic.

The optional injection queue is the bounded entry point for tasks submitted from external threads and for cross-worker
wakeups. A new `spawn()` reports an error if that queue is full. A fiber that is already active never fails merely
because the queue is full: its wakeup uses the scheduler's intrusive spill path, and workers prioritize that spill so
existing work continues to make progress. `FiberSchedulerDiagnostics` exposes the configured capacity, current and
peak occupancy, and spill count; peak and spill values remain available after `join()`.

# Choosing Task and Stack Storage

For small fixed workloads, `FiberTaskPool(Span<FiberTask>, Span<char>, stackSize)` partitions one buffer into stack
slots. Every slot reserves its entire stack in physical memory, which is simple but can become expensive at high
capacities. Stack size is a hard correctness limit, not just a performance knob; overflowing it is not recoverable.

For larger systems, the draft API also has reusable allocator-backed task records and virtual-memory-backed stack slots:

```cpp
FiberAllocator allocator;
FiberAllocatorVirtualOptions allocatorOptions;
allocatorOptions.reserveBytes       = 8 * 1024 * 1024;
allocatorOptions.initialCommitBytes = 64 * 1024;
SC_TRY(allocator.createVirtual(allocatorOptions));

FiberTaskClass taskClass;
FiberTaskClassOptions taskOptions;
taskOptions.maxTasks = 1024;
SC_TRY(taskClass.create(allocator, taskOptions));

FiberStackClass stackClass;
FiberStackClassOptions stackOptions;
stackOptions.stackSizeInBytes = FiberStackSize::ThirtyTwoKiB;
stackOptions.maxStacks        = 1024;
stackOptions.guardPage        = true;
SC_TRY(stackClass.reserve(stackOptions));

FiberTaskPool pool;
SC_TRY(pool.create(taskClass, stackClass));
```

`FiberStackClass` reserves fixed-size virtual slots, optionally with guard pages, and reports committed and high-water
usage. `FiberTaskClass` bounds reusable task records. Both must outlive every slot acquired from them, and close-time
validation rejects live allocations or task/stack slots.

# Stack Class Sizing

`FiberStackSize` names common requested sizes for virtual stacks: `FourKiB`, `EightKiB`, `ThirtyTwoKiB`, and
`SixtyFourKiB`. These are inputs to `FiberVirtualStackOptions` and `FiberStackClassOptions`, not runtime-selected
profiles. The OS rounds requested stack and guard sizes to its page size, so `FiberStackClassDiagnostics` is the source
of truth for actual reservation and committed bytes.

Use 4 KiB or 8 KiB only for shallow, measured procedures. 32 KiB is a reasonable starting point for dense cooperative
workloads, while 64 KiB remains the conservative default. Stack requirements include ordinary nested calls, C++
temporaries, and any library work below a suspension point. Measure high-water use with `fillHighWaterMarks()` before
reducing a production stack class, and retain enough margin for platform and build-mode variation.

# Waiting, Coordination, and Cancellation

Fiber primitives suspend the current fiber instead of blocking the OS thread:

- `FiberCounter` waits for a counted set of operations to complete.
- `FiberEvent` wakes all waiters when signaled.
- `FiberAutoResetEvent` wakes one waiter per signal.
- `FiberSemaphore` controls access to a fixed number of logical slots.
- `FiberMutex` protects cooperative fiber critical sections and diagnoses recursive or wrong-owner use.
- `FiberTaskGroup` spawns child tasks and collects errors without dynamic allocation.

Primitive waits and `FiberMutex::lock()` / `unlock()` must run inside a fiber owned by the supplied scheduler. Event and
semaphore signals may be published from another thread through that scheduler. Mutex ownership transfers to the chosen
waiter before its wake is published, so the previous owner cannot unlock twice or let another fiber enter during the
handoff window.

A task group retains the completed task records from one wave so `countErrors()` and `collectErrors()` can report stable
task identities even when the tasks came from a reusable pool. After waiting and inspecting results, call
`FiberTaskGroup::reset()` to release those records back to fixed or class-backed pools. Reset fails while tasks are
pending, and a group does not accept a new wave until the completed wave has been reset. Stack slots are released at
normal root-context completion; only the smaller task records remain retained for result inspection.

For example, a semaphore can bound how many fibers enter a cooperative region:

```cpp
FiberSemaphore limit(4);

SC_TRY(group.spawn(pool, FiberTask::Procedure(
                             [&limit](FiberScheduler& scheduler)
                             {
                                 SC_TRY(limit.wait(scheduler));
                                 Result result = doWork();
                                 SC_TRY(limit.signal(scheduler));
                                 return result;
                             })));
```

Cancellation is cooperative. `FiberCancellationTokenSource` can request cancellation for a group of spawned tasks, and
the scheduler wakes interruptible waits so tasks can return an error `Result`.

```cpp
FiberCancellationTokenSource cancelSource;
FiberTaskSpawnOptions        options;
options.cancellationToken = cancelSource.token();

SC_TRY(pool.spawn(scheduler, makeCancellableJob(), options));
SC_TRY(scheduler.requestCancel(cancelSource));
```

Tasks should still return plain `Result`; there is no exception dependency and no hidden cancellation object allocation.
Cancellation cannot interrupt arbitrary computation: task code must reach a cancellation-aware wait or explicitly
check its token.

# Lifetime and Thread-Local State

`FiberTask` execution is not pinned to the OS thread that first started the task. A task may yield, become ready again,
and later resume on a different worker thread, for example after work stealing or when another thread drives the same
`FiberScheduler`.

Do not use C++ `thread_local` variables or platform TLS to store logical fiber task state. Those values belong to the
current OS thread, not to the fiber, so a resumed task may observe a different value than it wrote before suspension.
Use explicit task state instead, such as captured state in the task procedure, caller-owned objects, or
`FiberTask::userData()`.

The task object, its stack, procedure captures, `userData`, synchronization primitives, and any state reached through
them must remain alive and at stable addresses until the task completes. A `FiberTaskGroup` helps join related work but
does not make borrowed state owning. Destroying a scheduler or storage class while work is active is a programming
error diagnosed by the library.

`FiberScheduler::shutdown()` is a reusable cancellation drain, not a permanent close. It requests cancellation for the
currently active tasks and drives them back to completion. Calling it again with no active work succeeds, and new tasks
may be spawned afterward. A worker pool has a separate lifecycle: `requestStop()` wakes its workers and `join()` must
finish before worker, thread, deque, or injection storage is reused or destroyed.

# Allocation Policy

`Fibers` follows the Sane C++ allocation rules:

- no hidden dynamic allocation in normal scheduling paths;
- tasks, stacks, workers, and thread handles are caller-owned;
- queue/deque storage is explicit through `FiberAllocator` when enabled;
- `FiberAllocator::createMalloc()` exists only as an explicit opt-in mode;
- virtual stack storage uses explicit reservation and capacity limits;
- close-time validation catches live allocations or live task/stack slots.

This means the runtime applies backpressure instead of silently growing. Producers provide enough capacity, wait for
capacity, or handle setup/allocation failure. `FiberAllocator::createMalloc()` is an explicit interoperability escape
hatch; choosing it gives up the fixed-memory property even though allocation remains visible in configuration and
statistics.

# Relationship to Neighboring Libraries

[Async](@ref library_async) is the low-level callback I/O library. [Await](@ref library_await) is a C++20 coroutine
wrapper over `Async`. `Fibers` is different: it is stackful, does not require C++20 coroutines, and can suspend through
ordinary nested function calls because each fiber has an explicit stack.

I/O integration is intentionally not part of `Fibers`; it lives in [FibersAsync](@ref library_fibers_async), which
depends on both `Fibers` and `Async`.

[Threading](@ref library_threading) is the lower-level choice when work naturally maps to OS threads or must block in
foreign code. Fibers trade a per-task stack and cooperative scheduling discipline for the ability to suspend through
ordinary nested functions. Await trades that stack for compiler-generated coroutine frames and requires C++20.

# API Reference

The complete class and method reference is in [the Fibers API group](@ref group_fibers). Start with
[FiberScheduler](@ref SC::FiberScheduler), [FiberTaskPool](@ref SC::FiberTaskPool),
[FiberTaskGroup](@ref SC::FiberTaskGroup), and [FiberWorkerPool](@ref SC::FiberWorkerPool). Storage-heavy deployments
should also read [FiberTaskClass](@ref SC::FiberTaskClass), [FiberStackClass](@ref SC::FiberStackClass), and their
diagnostics types.

# Further Examples

- `Examples/FibersDemo` shows a tiny CPU fiber workload, `FibersAsync` sleeps, and worker-pool I/O.
- `Examples/FibersBenchmark` contains explicit benchmark-style workloads for yield/resume and sustained micro-tasking.
- `Tests/Libraries/Fibers/FibersTest.cpp` is the best source of focused examples for cancellation, primitives,
  task pools, worker pools, work stealing, diagnostics, virtual stacks, and allocator-backed storage.

# Status

🟥 Draft

Current support includes:

- caller-provided `FiberStack` memory;
- virtual stack reservation and fixed-size `FiberStackClass` pools;
- internal context creation and switching on macOS, Linux, and Windows for supported 64-bit architectures;
- caller-owned `FiberTask` objects and allocator-backed `FiberTaskClass` storage;
- fixed-storage and class-backed `FiberTaskPool`;
- single-threaded `FiberScheduler` spawn, run, yield, and no-progress detection;
- worker-pool execution with work stealing and optional allocator-backed worker deques;
- scheduler, worker, stack, pool, and allocator diagnostics;
- optional tracing hooks with no allocation on the hot path;
- `FiberCounter` wait from both fibers and the root caller;
- `FiberTaskGroup` convenience spawning, wait-all result reporting, cancel-on-error, and pool-backed bounded fan-out;
- cooperative `FiberEvent`, `FiberAutoResetEvent`, `FiberSemaphore`, and `FiberMutex` primitives;
- cooperative task cancellation, including waking tasks suspended on counters and primitives;
- focused `SCTest` coverage for the raw context switch layer, scheduler primitives, worker pools, and storage classes.

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Fibers`. Single File and Standalone
counts are regenerated with the amalgamated library outputs.
