# Fibers Architecture

## Purpose

`Fibers` is the stackful cooperative execution runtime for Sane C++ Libraries. It should run large numbers of small CPU
tasks over caller-selected worker threads while preserving SC rules: no hidden dynamic allocation, no exceptions for
control flow, explicit storage budgets, stable task and stack ownership, and `Result` for errors only.

Future changes should make the runtime increasingly competitive for micro-tasking and work-stealing workloads without
turning it into an allocating general-purpose job system.

## Architectural Shape

`FiberScheduler` owns scheduling state but not task or stack memory. Callers provide `FiberTask` objects, stack storage,
worker storage, thread storage, task classes, stack classes, and allocator storage explicitly. `FiberWorkerPool` runs
caller-provided `FiberWorker` objects on caller-provided `FiberWorkerThread` objects and can use allocator-backed
per-worker deques for bounded work stealing.

`FiberTaskPool` is the ergonomic bounded facade for pairing task slots and stack slots. It can use direct fixed storage
or class-backed storage through `FiberTaskClass` and `FiberStackClass`. Completed pooled fibers return task and stack
slots only after the fiber has switched back to a worker root context, preserving safe reuse of non-movable fiber state.

## Boundaries

`Fibers` owns stackful context switching, cooperative scheduling, task lifecycle, worker-pool execution, work stealing,
cooperative synchronization primitives, cancellation wakeups, explicit fiber allocation helpers, diagnostics, and
no-allocation tracing hooks.

`Fibers` does not own async I/O, descriptor readiness, coroutine frames, protocol buffering, stream composition, hidden
thread pools, or unbounded heap-backed queues. I/O integration belongs in `FibersAsync`, and coroutine integration
belongs in a later explicit adapter only if a real need appears.

Logical fiber task state also does not belong in C++ `thread_local` variables or platform TLS. Fibers can resume on a
different worker thread, so logical state must live in explicit task/caller-owned storage instead.

## Similarities With Other Libraries

Like `Async`, `Fibers` treats public objects as in-flight storage whose lifetime must remain stable. Like `Await`, it
makes runtime memory explicit through a library-local allocator strategy. Like the rest of SC, it keeps `Result` focused
on errors, avoids exceptions and RTTI, and keeps dependency boundaries deliberate.

## Differences From Other Libraries

Unlike `Threading`, `Fibers` schedules many cooperative execution contexts over a smaller number of OS threads. Unlike
`Async`, it schedules CPU execution rather than native I/O completions. Unlike `Await`, it does not use C++20 coroutine
frames and does not need standard coroutine machinery.

## Inspirations

The implementation should follow proven fiber-runtime ideas: stackful suspension points, bounded worker-local deques,
owner push/pop with thief stealing, cooperative synchronization, and explicit stack budgeting. The API should remain
Sane C++ shaped rather than copying another runtime's ownership or allocation model.

## Anti-Inspirations

Do not add hidden heap allocation, shared-ownership task handles, implicit global schedulers, unbounded submission
queues, exception-based task failure, or APIs that make task or stack lifetime look detached from caller storage.

Do not add dependencies on `Async`, `Await`, or `Threading` to the core library unless a future decision explicitly
authorizes it because the dependency replaces substantial duplicated implementation.

## Architectural Choices

- Keep `Fibers` as an independent CPU runtime with caller-owned storage.
- Use `FiberAllocator`, not `AwaitAllocator`, for scheduler-specific explicit allocation.
- Keep fiber stack-size classes caller-selected and page-rounding-aware; do not infer them from task procedures.
- Retain full stack commitment on acquisition until a fault-handled incremental-commit prototype proves portable guard,
  sanitizer, debugger, and signal/exception behavior.
- Use bounded deterministic victim sampling for stealing rather than scanning every worker on every idle attempt.
- Keep yield and wake publication allocation-free for existing fibers.
- Apply capacity pressure primarily when creating new work, task slots, stack slots, or bounded queue storage.
- Keep cancellation cooperative and wake-based; do not preempt or unwind running fiber stacks.
- Use stack-local waiter nodes for cooperative waits instead of allocating waiter objects.
- Use bounded per-worker deques for work stealing and fall back to an intrusive global ready list when needed.
- Preserve the two-phase suspension model so a fiber is not made ready again until after it switches to the worker root.
- Keep logical task state out of OS-thread-local storage because fibers may migrate between worker threads.
- Keep diagnostics and tracing allocation-free and cheap enough to leave compiled into normal builds.
- Isolate bounded external injection and its pre-claim active registry from scheduler-global coordination; retain exact
  capacity and cancellation behavior while measuring the path independently.

## Explicitly Excluded Targets

- Hidden fallback allocation for task records, stacks, deques, or waiters.
- Unbounded job submission from external producers.
- Async I/O APIs in the core `Fibers` library.
- Coroutine-frame ownership or direct `AwaitTask` integration in the core runtime.
- Runtime-selectable I/O backends inside `Fibers`.

## Sources

- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
- [Fibers public interface](../../Libraries/Fibers/Fibers.h)
- [Fibers implementation](../../Libraries/Fibers/Fibers.cpp)
- [Fibers tests](../../Tests/Libraries/Fibers/FibersTest.cpp)
- [Fibers scale-up plan](../../Documentation/Plans/FibersPlan.md)
- [Project principles](../../Documentation/Pages/Principles.md)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0005 - Avoid STL, exceptions, and RTTI in library code](../Global/sc-0005-avoid-stl-exceptions-and-rtti-in-library-code.md)
- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)

## Decision Log

- [FIBERS-0001 - Keep Fibers independent from Async Await and Threading](fibers-0001-keep-fibers-independent-from-async-await-and-threading.md)
- [FIBERS-0002 - Use explicit FiberAllocator storage for scalable runtime memory](fibers-0002-use-explicit-fiberallocator-storage-for-scalable-runtime-memory.md)
- [FIBERS-0003 - Keep task and stack lifetimes caller-owned and memory-stable](fibers-0003-keep-task-and-stack-lifetimes-caller-owned-and-memory-stable.md)
- [FIBERS-0004 - Use bounded worker deques with intrusive global spill for work stealing](fibers-0004-use-bounded-worker-deques-with-intrusive-global-spill-for-work-stealing.md)
- [FIBERS-0005 - Keep logical fiber state out of thread-local storage](fibers-0005-keep-logical-fiber-state-out-of-thread-local-storage.md)
- [FIBERS-0006 - Keep cancellation cooperative and wake-based](fibers-0006-keep-cancellation-cooperative-and-wake-based.md)
- [FIBERS-0007 - Model spawn backpressure as explicit capacity waiting](fibers-0007-model-spawn-backpressure-as-explicit-capacity-waiting.md)
- [FIBERS-0008 - Use stack-local waiter nodes for cooperative waits](fibers-0008-use-stack-local-waiter-nodes-for-cooperative-waits.md)
- [FIBERS-0009 - Keep FiberTaskPool as the ergonomic facade over task and stack classes](fibers-0009-keep-fibertaskpool-as-the-ergonomic-facade-over-task-and-stack-classes.md)
- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [FIBERS-0011 - Keep fiber stack-size classes explicit](fibers-0011-keep-fiber-stack-size-classes-explicit.md)
- [FIBERS-0012 - Require a prototype before incremental fiber stack commitment](fibers-0012-require-a-prototype-before-incremental-fiber-stack-commitment.md)
- [FIBERS-0013 - Use bounded deterministic work-stealing victim sampling](fibers-0013-use-bounded-deterministic-work-stealing-victim-sampling.md)
- [FIBERS-0014 - Use bounded worker idle spinning](fibers-0014-use-bounded-worker-idle-spinning.md)
- [FIBERS-0015 - Retain task group records until explicit reset](fibers-0015-retain-task-group-records-until-explicit-reset.md)
- [FIBERS-0016 - Transfer fiber mutex ownership before wake publication](fibers-0016-transfer-fiber-mutex-ownership-before-wake-publication.md)
- [FIBERS-0017 - Gate external benchmarks behind optional packages](fibers-0017-package-gated-external-benchmarks.md)
- [FIBERS-0018 - Separate injection control from scheduler coordination](fibers-0018-separate-injection-control-from-scheduler-coordination.md)
