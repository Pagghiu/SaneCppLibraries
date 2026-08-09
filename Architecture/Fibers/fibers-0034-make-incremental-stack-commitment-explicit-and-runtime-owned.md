# FIBERS-0034 - Make Incremental Stack Commitment Explicit And Runtime-Owned

Status: Accepted
Date: 2026-08-01

## Context

FIBERS-0012 kept production stack classes fully committed until an isolated prototype proved bounded fault-driven
growth, terminal guard overflow, unrelated and nested fault forwarding, concurrent workers, and handler teardown on
macOS, Linux, and Windows. The prototype now passes on macOS ARM64 and x86_64, Linux ARM64 and x86_64, and Windows
x86_64 where runners are available.

The proven mechanism has two lifetimes that do not belong inside an individual stack slot. POSIX fault actions are
process-wide and each participating thread needs an alternate signal stack. Windows growth uses thread stack metadata
and guard pages. Hiding either lifetime inside `FiberStackClass` would make reservation mutate process state, obscure
thread-affine teardown, and encourage implicit allocation.

## Decision

Add production incremental commitment as an explicit opt-in capability with three separate ownership layers.

- `FiberStackClassOptions` selects full or incremental commitment. Full commitment remains the default and preserves
  existing behavior and tooling compatibility.
- Incremental options specify caller-selected initial commitment and growth increments. Both are page-rounded,
  validated against the usable stack size, and exposed through diagnostics. Windows uses an effective minimum of two
  pages per growth interval so native exception dispatch retains one writable page below the guard. Reservation and
  maximum stack count remain explicit exactly as they are for full commitment.
- A caller-owned `FiberStackGrowthRuntime` owns process-wide fault integration. Creating and closing it are explicit
  fallible operations. Closing is rejected while any participating thread remains registered.
- A caller-owned `FiberStackGrowthThread` owns one thread registration and, on POSIX, caller-provided alternate signal
  stack storage. It may only be closed by its installing thread. `FiberWorkerPool` receives bounded arrays and signal
  storage through its options and manages these thread records around worker entry and exit; manually driven threads
  use the same object explicitly.
- Immediately before entering an incremental stack, the scheduler publishes only that stack's immutable reservation
  bounds plus mutable committed boundary in thread-local fault state. It clears that pointer immediately after returning
  to the worker root. The fault path never searches a global registry and never takes scheduler, allocator, tracing, or
  stack-class locks.
- A valid growth fault must lie in the next bounded uncommitted interval and the interrupted stack pointer must lie in
  the same reservation. The handler commits at most one configured increment and resumes execution. Exhausted
  reservation remains a terminal guard fault; unrelated and nested faults are forwarded to the previously installed
  platform handler.
- The POSIX path deliberately relies on the validated macOS/Linux signal-context behavior of `mprotect`; it is not a
  portable claim about every POSIX implementation. Signal-side accounting is compile-time restricted to lock-free
  native `size_t` atomics so the handler cannot fall back to a runtime lock or allocation.
- Released slots decommit their usable range and reset their committed boundary before re-entering the free list.
  Diagnostics report metadata commitment plus each active slot's actual commitment, not its reservation.

The first production slice applies only to `FiberStackClass`. Standalone `FiberVirtualStack` growth remains fully
committed until a consuming use case demonstrates a lifecycle that is not already covered by stack classes.

## Tooling Policy

Incremental mode is unavailable under AddressSanitizer and while a debugger owns fault delivery. Runtime creation must
return an error before installing handlers in either case. Full-commit stack classes remain supported and unchanged.

Attaching a debugger after incremental execution starts is outside the supported mode; documentation must require the
runtime to be closed before attaching. No implementation may alter debugger settings or consume faults on its behalf.

## Capacity And Allocation

All metadata, thread records, and POSIX alternate signal stacks are caller-funded and bounded before workers start.
Starting a pool fails transactionally when worker records or signal storage are insufficient. No fallback allocation,
lazy growth of metadata, unbounded registry, or hidden global scheduler is permitted.

The runtime may reserve large virtual ranges while committing only metadata, configured initial stack pages, and pages
reached by validated growth faults. Virtual reservation is capacity, not allocation permission.

## Consequences

Large suspended populations can use stack reservations sized for exceptional depth while paying physical commitment
for observed depth. Callers also accept an explicit process/thread integration and must budget alternate signal storage
on POSIX.

Full commitment remains the simple portable mode for debuggers, sanitizers, embedders that cannot install process fault
handlers, and applications that prefer deterministic physical commitment over density.

The public Draft layouts will grow when the runtime and thread records are introduced. Binary consumers must recompile;
no compatibility shim is required while `Fibers` remains Draft.

## Implementation Order

1. Extract the platform fault core from `FibersStackGrowthPrototype` without changing the prototype's independent
   oracle behavior.
2. Add runtime/thread lifecycle and transactional worker-pool startup rollback with deterministic foreign-thread and
   active-thread teardown tests.
3. Add incremental `FiberStackClass` metadata, acquisition, release, diagnostics, and capacity tests.
4. Publish the active stack around context switches and repeat terminal, foreign, nested, cancellation, migration, and
   shutdown tests through the production scheduler.
5. Measure 10K and 100K suspended fibers with equal reservation sizes against full commitment before making a density
   claim.

## Confirmation

This decision is confirmed when full commitment is behaviorally unchanged; incremental mode passes the prototype's
fault matrix through production APIs on every available OS/architecture runner; worker startup and shutdown roll back
all caller storage after partial failure; and diagnostics reconcile exactly with committed pages after acquire, growth,
suspension, migration, completion, cancellation, and release.

## Related

- [FIBERS-0002 - Use explicit FiberAllocator storage for scalable runtime memory](fibers-0002-use-explicit-fiberallocator-storage-for-scalable-runtime-memory.md)
- [FIBERS-0011 - Keep fiber stack-size classes explicit](fibers-0011-keep-fiber-stack-size-classes-explicit.md)
- [FIBERS-0012 - Require a prototype before incremental fiber stack commitment](fibers-0012-require-a-prototype-before-incremental-fiber-stack-commitment.md)
- [Fibers architecture](fibers-architecture.md)
