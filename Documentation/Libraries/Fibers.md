@page library_fibers Fibers

@brief 🟥 Stackful cooperative task runtime

[TOC]

[SaneCppFibers.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFibers.h) is an experimental
stackful task runtime: code can suspend from an ordinary nested call stack without blocking the OS thread that runs it.
Tasks, stacks, workers, and bounded queues remain explicit program-owned resources.

For CPU work that never suspends, the library also includes an early `FiberJob` prototype. Jobs are stackless,
run-to-completion records with a separately bounded scheduler, so they do not pay for fiber stacks or weaken the
stackful scheduling contract.

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

# Stackless Jobs

`FiberJob` is for small CPU callbacks that never yield or wait. The caller supplies fixed ready-queue storage and can
either drive the scheduler explicitly or attach a bounded worker pool. Job failures remain on the completed job, while
`run()` reports scheduler failures.

```cpp
FiberJobScheduler jobScheduler;
FiberJob*         readyStorage[32] = {};
FiberJob          jobs[32];

SC_TRY(jobScheduler.create(readyStorage));
for (FiberJob& job : jobs)
{
    SC_TRY(jobScheduler.spawn(
        job, FiberJob::Procedure([](FiberJobContext& context)
                                 {
                                     SC_TRY(context.checkCancellation());
                                     return Result(true);
                                 })));
}
SC_TRY(jobScheduler.run());
SC_TRY(jobScheduler.close());
```

The queue never allocates or grows. `spawn()` reports capacity exhaustion, and a running job may submit children only
while a slot is available. `FiberJobContext` intentionally has no `yield()` or fiber synchronization API. Parallel job
workers execute and steal accepted jobs without reserving per-job stacks. Recursive help-while-full fan-out remains
future Draft work; use `FiberTask` whenever the callback must suspend or call `AsyncFibers`.

`FiberJobPool` provides O(1) acquisition from a fixed `Span<FiberJob>`. A completed job remains retained until the
caller has inspected `result()` and calls `release()`. This makes result lifetime and backpressure explicit: failed
scheduler publication immediately returns the attempted record, while unreleased completed records continue to count
against pool capacity. For allocator-backed storage, create a `FiberJobClass` with an explicit `FiberAllocator` and
bind the pool with `pool.create(jobClass)`. The class owns only stable record storage; the pool contract stays the same.

`FiberJobGroup` submits one bounded wave through a pool, drives its scheduler with `run()`, and retains every job result
for `countErrors()` or caller-provided `collectErrors()` storage. Call `reset()` after inspection to return all records
to their originating pools. A group never allocates and cannot start another completed wave before reset. Internal pool
retention and group membership are serialized in preparation for parallel workers, and group ownership is established
before scheduler publication. Group wave construction and result inspection remain caller-driven lifecycle operations;
do not mutate one group concurrently.

`FiberJobWorker` is a caller-owned parallel execution record. Its bounded local deque storage is
created explicitly through `FiberJobScheduler::createWorkerDeques()` and a `FiberAllocator`; partial setup failure
rolls back every deque allocated by that call. The worker overloads of `runOne()` and `run()` provide deterministic
manual execution: jobs spawned by a running worker publish to its local deque, and another supplied worker can steal
from the opposite end. Once worker-local work exists, callers must continue with a worker overload; plain `run()`
reports that it cannot drain local deques. The bounded external queue is serialized independently, so caller-managed
threads may invoke worker `runOne()` concurrently and execute jobs in parallel.

Callers that own contiguous stable records can publish them transactionally with
`FiberJobScheduler::spawn(Span<FiberJob>, Procedure)`. An external producer validates the entire span and bounded queue,
copies the fixed-size procedure into each record under one queue lock, updates accounting once, and wakes the pool as a
batch. A running job instead places the complete batch on its owner deque when it fits, with one accounting update and
one deque-bottom publication, then wakes one parked peer to steal from the opposite end. Later publications recruit
more peers without broadcasting at every recursive fan-out node. If the complete batch does not fit locally, the
scheduler uses the bounded external transaction rather than splitting the call. Empty spans, active records, invalid
pooled records, or insufficient capacity return an error without partial publication.

`FiberJobWorkerPool` adds library-owned OS threads without depending on `Threading`. Worker records and thread records
remain caller-owned, while every local deque comes from the explicit `FiberAllocator` in the options. By default,
`join()` drains one accepted wave. Set `keepAliveWhenIdle` to keep the same threads and deques parked between waves;
`waitIdle()` then marks each wave boundary, and `requestStop()` is required before the final `join()`. Stop wakes every
parked worker and makes cancellation observable through `FiberJobContext`. Multi-worker pools transfer claimed batches
to cache-line-isolated worker ready and active counters, so local execution and completion do not contend on
scheduler-wide counters. `readyJobCount()` and `activeJobCount()` are exact at stable observation points and can only
conservatively overcount during a concurrent ownership transfer; neither reports a false zero. No stack is reserved for
a job.

```cpp
FiberJobWorkerPoolOptions options;
options.dequeAllocator         = &allocator;
options.dequeCapacityPerWorker = 256;

SC_TRY(jobScheduler.spawn(job, procedure));
SC_TRY(workerPool.start(jobScheduler, workers, threads, options));
SC_TRY(workerPool.join());
```

Long-lived runtimes can accept repeated bounded waves without recreating their OS threads:

```cpp
FiberJobWorkerPoolOptions options;
options.dequeAllocator         = &allocator;
options.dequeCapacityPerWorker = 256;
options.keepAliveWhenIdle      = true;

SC_TRY(workerPool.start(jobScheduler, workers, threads, options));
SC_TRY(jobScheduler.spawn(firstJob, firstProcedure));
SC_TRY(workerPool.waitIdle());
SC_TRY(jobScheduler.spawn(secondJob, secondProcedure));
SC_TRY(workerPool.waitIdle());
SC_TRY(workerPool.requestStop());
SC_TRY(workerPool.join());
```

`waitIdle()` observes that all work accepted before that idle point has completed. External producers must be stopped
or otherwise coordinated if the caller needs a closed submission boundary; a concurrent later spawn begins a new wave.
A persistent pool rejects `join()` until stop has been requested, avoiding an accidental indefinite wait while it is
still accepting work.

`Examples/FibersMandelbrot` applies this model to a bounded image renderer. It preallocates one stable job per possible
row, starts a persistent worker pool, transactionally publishes the requested rows, waits for the wave, and then
inspects every retained result before writing the image:

@snippet Examples/FibersMandelbrot/FibersMandelbrot.cpp FibersMandelbrotRun

The example deliberately keeps worker count explicit. Its job, ready-queue, pixel, worker, thread, and deque capacities
all have fixed maxima, and invalid dimensions fail rather than growing them. This is the intended `FiberJob` shape for
independent CPU records that cannot suspend. Its command-line positional array is also stable storage; assigning a
temporary braced span to `CommandLineSpec::positionals` would leave a dangling borrowed view before parsing.

One batch procedure is copied into every submitted record. The renderer therefore uses `context.job()` to map each
stable record to its row in caller-owned sidecar storage. This preserves compact records and transactional publication,
but is less direct than per-record payload fields; keep that tradeoff in mind when choosing between one shared batch
procedure and individually captured job procedures.

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

SC_TRY(workerPool.start(scheduler, workers, threads));
SC_TRY(workerPool.join());
```

For higher-throughput scheduling, explicit deque storage can be provided through `FiberAllocator`. The fixed allocator
uses a caller buffer; the virtual allocator reserves a caller-selected address-space budget and commits pages on demand.

```cpp
char           allocatorStorage[64 * 1024] = {};
FiberAllocator allocator;
SC_TRY(allocator.createFixed(allocatorStorage));

FiberWorkerPoolOptions options;
options.dequeAllocator         = &allocator;
options.dequeCapacityPerWorker = 256;
options.injectionAllocator     = &allocator;
options.injectionCapacity      = 256;

SC_TRY(workerPool.start(scheduler, workers, threads, options));
SC_TRY(workerPool.join());
SC_TRY(allocator.close());
```

The worker pool owns OS threads while running, but the memory for workers, thread handles, deques, tasks, and stacks is
still selected by the caller. `join()` waits for scheduled work and worker shutdown; lifecycle ordering is therefore
part of application shutdown rather than destructor magic. If `start()` fails after creating allocator-backed queues,
it releases every partial queue allocation and leaves the pool, scheduler, worker/thread storage, and allocator
reusable.

The optional injection queue is the bounded entry point for tasks submitted from external threads and for cross-worker
wakeups. A new `spawn()` reports an error if that queue is full. A fiber that is already active never fails merely
because the queue is full: its wakeup uses the scheduler's intrusive spill path, and workers prioritize that spill so
existing work continues to make progress. `FiberSchedulerDiagnostics` exposes the configured capacity, current and
peak occupancy, in-progress publications, spill count, and injection-control contention separately from scheduler
coordination; peak and spill values remain available after `join()`. Ordinary counter-free external spawns publish
through a slot-sequenced bounded queue, so producers and workers do not serialize on queue indices. Before publication,
each ordinary task receives a stable worker-registry home distributed by its injection position. Claiming or migrating
the task does not transfer that control-plane ownership. Injection control remains for coordinated fallback submissions
that use the global pre-claim registry. Fixed allocator budgets should include
`injectionCapacity * FiberInjectionSlotStorageSize` bytes in addition to worker deque storage and allocator alignment.

`FiberTaskPool::waitForAvailableTask()` provides one-slot backpressure. Producers that refill in batches can instead
call `waitForAvailableTasks(scheduler, minimumAvailable)` to suspend until that many task/stack pairs are reusable,
reducing wake/resuspend churn without allocating. The minimum must be between one and the pool capacity. Multiple
waiters remain FIFO, so a larger threshold at the head preserves arrival order rather than allowing a later smaller
request to bypass it. Cancellation removes a waiting fiber through the same cooperative waiter path.

Configured pools with peer workers transfer a bounded injection backlog into local stealable deques in larger batches
than the latency-sensitive spill path. Workers claim these slot-sequenced batches without taking the scheduler-global
ready lock. Ordinary externally submitted tasks already have stable cancellation-registry ownership, so claiming them
does not take injection control. A worker reserves the published contiguous prefix of each bounded batch with one head
transition and updates ready accounting once for the batch; claimed capacity is released to producers only after that
accounting is visible. The intrusive spill and coordinated fallback paths retain scheduler coordination.
`injectionClaimBatchPeak` exposes the observed transfer size for tuning and regression analysis.

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
validation rejects live allocations or task/stack slots. A class-backed `FiberTaskPool::spawn()` owns both a task and
a stack or neither: failed stack acquisition or scheduler publication returns every acquired slot, clears a provided
`outTask`, and leaves the pool reusable.

# Stack Class Sizing

`FiberStackSize` names common requested sizes for virtual stacks: `FourKiB`, `EightKiB`, `ThirtyTwoKiB`, and
`SixtyFourKiB`. These are inputs to `FiberVirtualStackOptions` and `FiberStackClassOptions`, not runtime-selected
profiles. The OS rounds requested stack and guard sizes to its page size, so `FiberStackClassDiagnostics` is the source
of truth for actual reservation and committed bytes.

Use 4 KiB or 8 KiB only for shallow, measured procedures. 32 KiB is a reasonable starting point for dense cooperative
workloads, while 64 KiB remains the conservative default. Stack requirements include ordinary nested calls, C++
temporaries, and any library work below a suspension point. Measure high-water use with `fillHighWaterMarks()` before
reducing a production stack class, and retain enough margin for platform and build-mode variation.

# Incremental Stack Lifecycle

Incremental execution is an opt-in Draft capability with production growth, migration, teardown, and isolated fault
coverage on the supported CPU/OS paths. Full commitment remains the simpler portable choice for configurations that
cannot give Fibers ownership of process fault handling.

Full commitment remains the `FiberStackClass` default. Opt-in `FiberStackCommitMode::Incremental` classes take explicit
non-zero `initialCommitSizeInBytes` and `growthCommitSizeInBytes`; both are page-rounded, bounded by the usable stack,
and exposed through `FiberStackClassDiagnostics`. Windows rounds the effective growth interval to at least two pages,
leaving one writable page below the native guard for exception dispatch. Acquisition commits only the initial high end
of the downward-growing stack, release decommits its actual committed interval, and committed-byte diagnostics include
metadata plus active slot commitment rather than virtual reservation.

The scheduler publishes the active incremental slot in thread-local execution state immediately before entering it and
clears that state immediately after returning to the worker root. A valid fault commits one bounded growth increment;
yielding and migration republish the same slot on the resuming OS thread. Completion and cancellation release the
slot's actual committed interval before it becomes reusable. On Windows, the first dispatch may commit one growth
increment while preparing the native guard page, so diagnostics rather than the requested initial size remain the
source of truth for physical commitment. Exhausting the reservation is always fatal; Windows can report either native
stack overflow when no further guard can be created or native access violation when the final stack probe crosses the
reservation boundary.

High-water diagnostics remain exact for fully committed classes. For an incremental stack they scan the initial
interval until growth occurs, then conservatively report its committed boundary; diagnostics never read a live guard
or uncommitted page and may therefore overestimate actual use within the committed interval.

The POSIX growth handler relies on the validated macOS and Linux signal-context behavior of page protection changes
and lock-free native accounting. Incremental execution is therefore supported only on the CPU/OS paths explicitly
qualified by the library rather than being implied by generic POSIX availability.

The POSIX action permits same-signal re-entry so a fault raised inside growth handling reaches the re-entrancy guard
and is forwarded to the previously installed handler. Windows uses native guard-page growth and leaves nested native
faults under OS exception dispatch.

One `FiberStackGrowthRuntime` owns the process-wide fault-handler integration. Every OS thread that may execute an
incremental stack will register one thread-affine `FiberStackGrowthThread`. On POSIX, registration requires at least
`FiberStackGrowthSignalStackSize` bytes of caller-owned alternate signal-stack storage; Windows accepts an empty span.
The thread registration and its storage must remain alive until `close()` runs on that same OS thread.

```cpp
FiberStackGrowthRuntime growthRuntime;
FiberStackGrowthThread  growthThread;

SC_TRY(growthRuntime.create());

#if SC_PLATFORM_WINDOWS
SC_TRY(growthThread.create(growthRuntime, {}));
#else
char signalStack[FiberStackGrowthSignalStackSize] = {};
SC_TRY(growthThread.create(growthRuntime, signalStack));
#endif

// Class-backed incremental tasks may execute while this registration is alive.

SC_TRY(growthThread.close());
SC_TRY(growthRuntime.close());
```

The runtime rejects shutdown while threads remain registered, competing process owners, foreign-thread registration
teardown, and replacement of a registered POSIX alternate signal stack. Creation is unavailable under AddressSanitizer
or an attached debugger because those tools also own fault handling. Full stack commitment remains the compatible
default, including under those tools. Dispatching an incremental class without a matching thread registration finishes
that task with an error before entering its stack.

Close every incremental growth runtime before attaching a debugger to the process. Attaching after incremental
execution starts is outside the supported tooling contract.

`FiberWorkerPool` can register and close all of its worker threads through `FiberWorkerPoolOptions`. Set
`stackGrowthRuntime` to the open process owner. On POSIX, also provide one flat
`stackGrowthSignalStackStorage` span containing at least `workerCount * FiberStackGrowthSignalStackSize` bytes; the
pool partitions it into stable per-worker spans without allocating. Startup rejects the complete configuration before
launching a worker if the budget is insufficient, and partial OS-thread startup rollback closes every registration
that was already created. `join()` leaves the runtime with zero registered threads, so the same worker/thread/storage
objects can start another wave immediately or the runtime can close.

# Waiting, Coordination, and Cancellation

Fiber primitives suspend the current fiber instead of blocking the OS thread:

- `FiberCounter` waits for a counted set of operations to complete.
- `FiberEvent` wakes all waiters when signaled.
- `FiberAutoResetEvent` wakes one waiter per signal.
- `FiberSemaphore` controls access to a fixed number of logical slots.
- `FiberMutex` protects cooperative fiber critical sections and diagnoses recursive or wrong-owner use.
- `FiberTaskGroup` spawns child tasks and collects errors without dynamic allocation.

Cooperative event, auto-reset event, semaphore, and mutex waits must run inside a fiber owned by the supplied scheduler.
`FiberCounter` is the deliberate exception: its scheduler wait can also drive ready work from the root caller. Event and
semaphore signals may be published from another thread through that scheduler. Mutex ownership transfers to the chosen
waiter before its wake is published, so the previous owner cannot unlock twice or let another fiber enter during the
handoff window. If a selected waiter is canceled before resuming, one-shot ownership from an auto-reset event,
semaphore, or mutex is transferred to another waiter or retained for the next wait rather than being lost.

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

Cancellation tokens borrow their `FiberCancellationTokenSource`; the source must outlive every token and every task
spawned with one. Treat `FiberCancellationTokenSource::reset()` as a between-wave lifecycle operation after all prior
token users have completed, not as a way to revoke cancellation from active work.

`FiberScheduler` serializes task publication, cancellation, and wake operations used by external producers. A fixed
span-backed `FiberTaskPool` does not serialize its slot scan or `nextTask` cursor, so calls that acquire/spawn pool slots
must come from one producer or be externally serialized; task execution and completion may still happen on many
workers. Class acquire/release operations have their own internal serialization. Reserve/create/close/release and
worker-pool start/join are quiescent lifecycle operations; `FiberWorkerPool::requestStop()` is the cross-thread stop
signal.

`FiberScheduler::shutdown()` is a reusable cancellation drain, not a permanent close. It requests cancellation for the
currently active tasks and drives them back to completion. Calling it again with no active work succeeds, and new tasks
may be spawned afterward. A worker pool has a separate lifecycle: `requestStop()` wakes its workers and `join()` must
finish before worker, thread, deque, or injection storage is reused or destroyed.

Normal ready publication logically requests one worker wake. The pool coalesces redundant operating-system signals
while an earlier signal is still pending, and skips the condition-variable mutex entirely when no worker has published
park intent. Every publication still atomically advances the wake generation so publication racing with parking cannot
be lost. Shutdown and the transition to no active fibers remain wake-all operations. Use
`FiberWorkerPool::wakeDiagnostics()` to compare logical `wakeNotifications` with the individual `wakeSignals` that
remain after coalescing; broadcasts are included only in `wakeNotifications`.

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

I/O integration is intentionally not part of `Fibers`; it lives in [AsyncFibers](@ref library_async_fibers), which
depends on both `Fibers` and `Async`.

[Threading](@ref library_threading) is the lower-level choice when work naturally maps to OS threads or must block in
foreign code. Fibers trade a per-task stack and cooperative scheduling discipline for the ability to suspend through
ordinary nested functions. Await trades that stack for compiler-generated coroutine frames and requires C++20.

# API Reference

The complete class and method reference is in [the Fibers API group](@ref group_fibers). Start with
[FiberScheduler](@ref SC::FiberScheduler), [FiberTaskPool](@ref SC::FiberTaskPool),
[FiberTaskGroup](@ref SC::FiberTaskGroup), [FiberWorkerPool](@ref SC::FiberWorkerPool), and the stackless
[FiberJobScheduler](@ref SC::FiberJobScheduler). Storage-heavy deployments
should also read [FiberTaskClass](@ref SC::FiberTaskClass), [FiberStackClass](@ref SC::FiberStackClass), and their
diagnostics types.

# Further Examples

- `Examples/FibersDemo` shows a tiny CPU fiber workload, `AsyncFibers` sleeps, and worker-pool I/O.
- `Examples/FibersMandelbrot` renders a bounded grayscale image with one stackless job per row and reports worker
  execution and stealing diagnostics.
- `Examples/FibersBenchmark` contains explicit benchmark-style workloads for yield/resume, sustained micro-tasking,
  raw stackless dispatch, and pooled stackless acquire/run/release overhead. Its
  `--job-worker-matrix --job-rounds <COUNT>` mode runs one warm-up and bounded repeated samples at unique
  1/2/4/8/hardware-worker counts, reporting minimum, median, mean, and maximum throughput without allocating sample
  storage. Its `--job-worker-sustained --job-workers <COUNT>` mode keeps one job worker pool alive and reuses a fixed
  8,192-record batch across one million jobs, timing transactional batch publication through each explicit idle
  boundary. It reports both the stackful-comparable shared-atomic payload and an independent per-record output variant
  that isolates scheduler throughput from application cache-line contention. Its equal-reservation density modes use
  `--mass-suspension <COUNT> --mass-suspension-commit full` and `incremental`; compare matching Release runs rather
  than unlike stack sizes or task counts. The output separates reservation, metadata and slot commitment, peak
  commitment, spawn/acquire, suspension, wake, and completion/slot-release boundaries.
- Worker-local `FiberJob` batch publication wakes one parked peer when it creates a new local backlog. Appending a
  batch behind work that is already visible still advances the wake generation, closing the publication-versus-park
  race, but avoids another condition-variable signal. Single-job publication retains wake-one behavior and external
  batch publication retains wake-all behavior.
- `Examples/FibersSkynetBenchmark` is enabled after `./SC.sh package install taskflow-benchmarks` and compares the
  stackful scheduler and stackless jobs with Taskflow's pinned upstream Skynet backend. The stackful depth limit is
  explicit because each live `FiberTask` owns a fixed stack; the `FiberJob` backend uses distinct stable continuation
  records and supports the canonical million-leaf workload without per-job stacks. Its persistent worker pool is reused
  across warm-up and measured waves at each depth. `--job-idle-spins <COUNT>` isolates the bounded worker spin policy
  when profiling parking and wake overhead; its default matches `FiberJobWorkerPoolOptions`.
- `Tests/Libraries/Fibers/FibersTest.cpp` is the best source of focused examples for cancellation, primitives,
  task pools, worker pools, work stealing, diagnostics, virtual stacks, and allocator-backed storage.

# Status

🟥 Draft

This Draft does not promise binary compatibility. The stabilization candidate changes public caller-sized layouts,
including task and spawn metadata, so binary consumers must recompile against the matching headers. No compatibility
shim is provided or required while the library remains Draft.

Current support includes:

- caller-provided `FiberStack` memory;
- virtual stack reservation and fixed-size `FiberStackClass` pools;
- internal context creation and switching on macOS, Linux, and Windows for supported 64-bit architectures;
- caller-owned `FiberTask` objects and allocator-backed `FiberTaskClass` storage;
- caller-owned stackless `FiberJob` records with a fixed-capacity, single-thread-driven scheduler prototype;
- fixed-storage or allocator-backed `FiberJobPool` reuse with explicit completed-result retention and release;
- bounded `FiberJobGroup` waves with cancellation, aggregate errors, and explicit pooled-record reset;
- caller-owned `FiberJobWorker` records with allocator-backed bounded deque storage and startup rollback;
- no-allocation `FiberJobWorkerPool` execution with work stealing, cooperative stop, and parked-worker wake-up;
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

# Blog

Development notes and design changes are recorded in the project updates:

- [July 2026 Update](https://pagghiu.github.io/site/blog/2026-07-31-SaneCppLibrariesUpdate.html)

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Fibers`.
Single File counts
`SaneCppFibers.h`.
Standalone counts `SaneCppFibersStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 1453		| 7731		| 9184	|
| Single File | 2207		| 7966		| 10173	|
| Standalone  | 2207		| 7966		| 10173	|
