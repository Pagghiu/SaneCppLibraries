# FIBERS-0012 - Require A Prototype Before Incremental Fiber Stack Commitment

Status: Accepted
Date: 2026-07-10

## Context

`FiberStackClass` can reserve a large virtual arena, but it currently commits a whole requested stack slot when that
slot is acquired. This is simple, portable, and makes a stack overflow hit a deterministic guard page, but it limits
the practical number of simultaneously live fibers by committed memory rather than reservation alone.

Incrementally committing a stack is not just a virtual-memory accounting optimization. Ordinary C++ code can cross an
uncommitted boundary at any nested call. Recovering from that access needs a platform fault mechanism that can identify
the owning stack, commit the next safe range without allocation or unsafe locks, restore execution, and distinguish a
growth fault from guard-page overflow or an unrelated access violation. macOS, Linux, Windows, sanitizers, and debuggers
have materially different constraints.

## Decision

Keep full requested-stack commitment on `FiberStackClass::acquire()` and preserve the current optional guard page.
Do not add a partially committed stack mode, a process-global signal handler, or a vectored exception handler to the
production library until a dedicated platform prototype proves the following contract:

- Stack metadata identifies the reserved range, committed range, guard range, and owning fiber without allocation.
- The fault path uses only operations valid in the relevant platform's signal or exception context, including no
  allocator, scheduler lock, tracing hook, or unsafe library call.
- A successful growth commits a bounded, caller-selected increment and continues only within the stack's reservation.
- A guard-page or exhausted-reservation fault remains deterministic and distinguishable from a growth fault.
- Nested fault, shutdown, foreign-thread, sanitizer, debugger, and concurrent-worker behavior are defined and tested.
- The prototype works on ARM64 and x86_64 macOS, Linux, and Windows before it becomes a `Fibers` API or build-path
  commitment.

The prototype belongs in an isolated support experiment with no public `Fibers` API. It must include explicit opt-in,
small controlled stacks, and process-level tests that demonstrate growth, guard overflow, and rejection of foreign
faults. A subsequent ADR can decide whether the proven mechanism belongs in `Fibers` and what caller-provided metadata
and platform restrictions it requires.

## Consequences

Current callers retain predictable capacity: each active class-backed stack consumes one page-rounded requested stack
plus any guard page, and diagnostics expose that commitment. Small explicit stack classes and high-water measurements
remain the supported way to increase density today.

The runtime avoids adding global process behavior whose interaction with embedding applications, sanitizers, and crash
reporting is unclear. The cost is that very large live populations remain bounded by committed memory until the
prototype demonstrates a safe alternative.

## Alternatives Considered

- Pre-commit only a small stack prefix without a growth handler: rejected because ordinary C++ execution would fault at
  an unhandled boundary.
- Remove guard pages to save memory: rejected because it weakens overflow diagnostics without solving commitment cost.
- Add a handler only on one platform: rejected because `Fibers` promises shared macOS/Linux/Windows semantics and the
  handler would be a global process integration point.
- Commit and decommit arbitrary ranges from normal scheduler code: rejected because the scheduler cannot run before the
  faulting stack has safely transferred control.

## Confirmation

A change preserves this decision when production stack acquisition fully commits its requested page-rounded slot,
guard behavior remains explicit, and any transparent-growth work first lives in a separately tested prototype that
meets every platform and fault-context requirement above.

## Related

- [FIBERS-0002 - Use explicit FiberAllocator storage for scalable runtime memory](fibers-0002-use-explicit-fiberallocator-storage-for-scalable-runtime-memory.md)
- [FIBERS-0003 - Keep task and stack lifetimes caller-owned and memory-stable](fibers-0003-keep-task-and-stack-lifetimes-caller-owned-and-memory-stable.md)
- [FIBERS-0011 - Keep fiber stack-size classes explicit](fibers-0011-keep-fiber-stack-size-classes-explicit.md)
- [Fibers architecture](fibers-architecture.md)
