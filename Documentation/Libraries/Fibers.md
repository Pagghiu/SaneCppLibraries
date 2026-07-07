@page library_fibers Fibers

@brief 🟥 Stackful cooperative task runtime

[TOC]

[SaneCppFibers.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFibers.h) is a stackful fiber runtime in Sane C++ style: explicit storage, stable objects, cooperative suspension, and no hidden dynamic allocation.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Fibers.svg)


# What Fibers Is For

`Fibers` is a CPU/tasking runtime for code that wants synchronous-looking control flow without blocking an OS thread.
A fiber owns a stack, can call normal C++ functions, and can cooperatively suspend with `FiberScheduler::yield()` or by
waiting on fiber primitives. Later it can resume on the same worker or on another worker.

The intended long-term shape is a small no-allocation runtime for micro-tasking workloads: many short jobs over time,
bounded pools of reusable `FiberTask` objects and stacks, work stealing between worker threads, and explicit memory
budgets chosen by the caller.

Use `Fibers` when you want:

- stackful tasks that can suspend from ordinary call stacks;
- caller-owned task, stack, worker, and queue storage;
- cooperative synchronization primitives such as events, semaphores, mutexes, counters, and task groups;
- a runtime that can run on one thread today and on a caller-provided worker pool when parallelism is useful;
- no dependency on [Async](@ref library_async), [Await](@ref library_await), or [Threading](@ref library_threading).

# Mental Model

`FiberScheduler` owns the logical scheduling state, but not the storage of the things it schedules. A task is made from:

- a caller-owned `FiberTask`;
- a caller-owned `FiberStack`, or a slot acquired through `FiberTaskPool`;
- a `FiberTask::Procedure` returning plain `Result`;
- optional cancellation, counter, and user-data inputs through `FiberTaskSpawnOptions`.

The scheduler runs ready tasks until they complete, yield, or wait. When a task yields or a wait is satisfied, it is
queued back as ready using intrusive links already present in `FiberTask`; normal yield/wake publication does not
allocate.

# A Small CPU Example

`FiberTaskPool` is the ergonomic way to run many bounded tasks without manually pairing each task with a stack. The
pool does not grow: if all slots are active, producers can wait for capacity and try again.

```cpp
struct State
{
    int partials[3] = {};
};

FiberScheduler scheduler;
FiberTask      tasks[3];
char           stackMemory[3 * 64 * 1024] = {};
FiberTaskPool  pool({tasks, 3}, {stackMemory, sizeof(stackMemory)}, 64 * 1024);
FiberTaskGroup group(scheduler);
State          state;

for (size_t taskIndex = 0; taskIndex < 3; ++taskIndex)
{
    SC_TRY(group.spawn(pool, FiberTask::Procedure(
                                 [&state, taskIndex](FiberScheduler& scheduler)
                                 {
                                     for (int value = 0; value < 5; ++value)
                                     {
                                         state.partials[taskIndex] += static_cast<int>(taskIndex + 1) * value;
                                         SC_TRY(scheduler.yield());
                                     }
                                     return Result(true);
                                 })));
}

SC_TRY(group.waitAll());
```

This is still ordinary C++ control flow. The call to `yield()` cooperatively gives another ready fiber a chance to run,
but no OS thread is blocked waiting for preemption.

# Bounded Fan-Out

When producing more work than the pool can hold at once, capacity pressure is explicit. From inside a fiber,
`waitForSpawnCapacity()` suspends cooperatively until at least one pool slot is available.

```cpp
while (hasMoreJobs())
{
    FiberTask* spawnedTask = nullptr;
    Result     spawned     = pool.spawn(scheduler, makeJob(), &spawnedTask);
    if (spawned)
    {
        continue;
    }

    SC_TRY(pool.waitForSpawnCapacity(scheduler));
    SC_TRY(pool.spawn(scheduler, makeJob(), &spawnedTask));
}
```

`Result` is still reserved for real errors or cancellation. Capacity is observable through `hasAvailableTask()` and
`availableCount()`, while waiting is modeled as an explicit scheduling operation.

# Worker Pools And Work Stealing

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

For higher-throughput scheduling, provide explicit deque storage through `FiberAllocator`:

```cpp
char           allocatorStorage[64 * 1024] = {};
FiberAllocator allocator;
SC_TRY(allocator.createFixed({allocatorStorage, sizeof(allocatorStorage)}));

FiberWorkerPoolOptions options;
options.dequeAllocator         = &allocator;
options.dequeCapacityPerWorker = 256;

SC_TRY(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, options));
SC_TRY(workerPool.join());
SC_TRY(allocator.close());
```

The worker pool owns OS threads while running, but the memory for workers, thread handles, deques, tasks, and stacks is
still selected by the caller.

# Scalable Task And Stack Storage

For simple examples, `FiberTaskPool(Span<FiberTask>, Span<char>, stackSize)` is often enough. For larger systems, the
draft API also has explicit classes for reusable task records and virtual-memory-backed stack slots:

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
stackOptions.stackSizeInBytes = 64 * 1024;
stackOptions.maxStacks        = 1024;
stackOptions.guardPage        = true;
SC_TRY(stackClass.reserve(stackOptions));

FiberTaskPool pool;
SC_TRY(pool.create(taskClass, stackClass));
```

This keeps the public memory budget explicit while preparing the runtime for large numbers of tasks over time.

# Synchronization Primitives

Fiber primitives suspend the current fiber instead of blocking the OS thread:

- `FiberCounter` waits for a counted set of operations to complete.
- `FiberEvent` wakes all waiters when signaled.
- `FiberAutoResetEvent` wakes one waiter per signal.
- `FiberSemaphore` controls access to a fixed number of logical slots.
- `FiberMutex` protects cooperative fiber critical sections and diagnoses recursive or wrong-owner use.
- `FiberTaskGroup` spawns child tasks and collects errors without dynamic allocation.

Example using a semaphore as a cooperative concurrency limit:

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

# Cancellation

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

# Thread-Local State

`FiberTask` execution is not pinned to the OS thread that first started the task. A task may yield, become ready again,
and later resume on a different worker thread, for example after work stealing or when another thread drives the same
`FiberScheduler`.

Do not use C++ `thread_local` variables or platform TLS to store logical fiber task state. Those values belong to the
current OS thread, not to the fiber, so a resumed task may observe a different value than it wrote before suspension.
Use explicit task state instead, such as captured state in the task procedure, caller-owned objects, or
`FiberTask::userData()`.

# Allocation Model

`Fibers` follows the Sane C++ allocation rules:

- no hidden dynamic allocation in normal scheduling paths;
- tasks, stacks, workers, and thread handles are caller-owned;
- queue/deque storage is explicit through `FiberAllocator` when enabled;
- `FiberAllocator::createMalloc()` exists only as an explicit opt-in mode;
- virtual stack storage uses explicit reservation and capacity limits;
- close-time validation catches live allocations or live task/stack slots.

This means the runtime can apply backpressure instead of silently growing. Producers either provide enough capacity,
wait for capacity, or receive a normal `Result` error when setup/allocation fails.

# Fibers vs Await vs Async

[Async](@ref library_async) is the low-level callback I/O library. [Await](@ref library_await) is a C++20 coroutine
wrapper over `Async`. `Fibers` is different: it is stackful, does not require C++20 coroutines, and can suspend through
ordinary nested function calls because each fiber has an explicit stack.

I/O integration is intentionally not part of `Fibers`; it lives in [FibersAsync](@ref library_fibers_async), which
depends on both `Fibers` and `Async`.

# Features

| Fibers API                                | Description                       |
|:------------------------------------------|:----------------------------------|
| [FiberStack](@ref SC::FiberStack)         | Caller-owned stack storage used by fiber contexts. |
| [FiberVirtualStack](@ref SC::FiberVirtualStack) | Virtual-memory-backed stack storage with an optional guard page. |
| [FiberStackClass](@ref SC::FiberStackClass) | Fixed-size virtual stack slot class with explicit capacity. |
| [FiberTask](@ref SC::FiberTask)           | Caller-owned task object scheduled by `FiberScheduler`. |
| [FiberTaskClass](@ref SC::FiberTaskClass) | Allocator-backed fixed-capacity storage for reusable `FiberTask` objects. |
| [FiberTaskPool](@ref SC::FiberTaskPool)   | Caller-owned pool pairing task objects with stack slots. |
| [FiberTaskGroup](@ref SC::FiberTaskGroup) | Helper for spawning child tasks and waiting for completion/errors. |
| [FiberCounter](@ref SC::FiberCounter)     | Counter used to suspend fibers until work completes. |
| [FiberEvent](@ref SC::FiberEvent)         | Manual-reset event that wakes waiting fibers when signaled. |
| [FiberAutoResetEvent](@ref SC::FiberAutoResetEvent) | Auto-reset event that wakes one waiting fiber per signal. |
| [FiberSemaphore](@ref SC::FiberSemaphore) | Counting semaphore for cooperative fibers. |
| [FiberMutex](@ref SC::FiberMutex)         | Cooperative mutex for fibers running on one scheduler. |
| [FiberAllocator](@ref SC::FiberAllocator) | Explicit allocator for scheduler/deque and scalable runtime storage. |
| [FiberWorker](@ref SC::FiberWorker)       | Caller-owned execution agent for running ready fibers on an OS thread. |
| [FiberWorkerPool](@ref SC::FiberWorkerPool) | No-allocation OS-thread-owning worker pool using caller-provided storage. |
| [FiberScheduler](@ref SC::FiberScheduler) | Cooperative scheduler with explicit workers and caller-owned storage. |

# Complete Examples

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

# Details

@copydetails group_fibers
