# FIBERS-0011 - Keep Fiber Stack-Size Classes Explicit

Status: Accepted
Date: 2026-07-10

## Context

Large populations of suspended fibers need small, predictable stack budgets. A requested stack size is not a portable
physical-memory promise: the runtime rounds stacks and optional guard pages to the active OS page size, and actual call
depth depends on the procedure, compiler, platform, and build configuration.

Leaving every common size as an unadorned integer makes code less legible and encourages callers to treat a requested
byte count as an automatic safety decision. Choosing a stack class from the procedure at runtime would hide policy,
require unreliable introspection, and conflict with caller-owned capacity planning.

## Decision

Expose `FiberStackSize::FourKiB`, `EightKiB`, `ThirtyTwoKiB`, and `SixtyFourKiB` as common requested sizes for
`FiberVirtualStackOptions` and `FiberStackClassOptions`. They are ordinary byte counts, preserving the existing options
APIs and allowing callers to supply any measured value that meets `FiberStackMinimumSize`.

Keep the existing 64 KiB defaults. The presets do not select an implicit stack class, reserve memory, commit pages, or
remove caller responsibility for measuring stack high-water use. `FiberStackClassDiagnostics` remains authoritative for
page-rounded stack size, guard size, reservation, and committed memory.

## Consequences

Callers can express common small-stack classes without a new allocator, dependency, or hidden policy. Dense workloads
can start from explicit 4 KiB, 8 KiB, or 32 KiB requests and validate their real platform footprint with diagnostics.
Existing callers retain compatible defaults and can continue to use arbitrary sizes.

The library deliberately does not promise that a named request fits every procedure. Incremental commitment and
fault-driven stack growth remain separate future work because their platform, sanitizer, debugger, and overflow
semantics need a dedicated design.

## Alternatives Considered

- Infer a size from a task procedure: rejected because C++ does not expose a reliable stack-depth bound and this would
  hide a caller-owned capacity decision.
- Use an enum-only size API: rejected because callers need arbitrary measured byte counts and should not need a cast to
  configure existing options.
- Make small stacks the new default: rejected because it changes established behavior before representative
  high-water measurements exist.

## Confirmation

A change preserves this decision when stack size remains caller-selected, named sizes remain requested byte counts,
diagnostics expose the rounded physical-memory consequences, and no stack sizing path performs hidden allocation or
automatic growth.

## Related

- [FIBERS-0002 - Use explicit FiberAllocator storage for scalable runtime memory](fibers-0002-use-explicit-fiberallocator-storage-for-scalable-runtime-memory.md)
- [FIBERS-0003 - Keep task and stack lifetimes caller-owned and memory-stable](fibers-0003-keep-task-and-stack-lifetimes-caller-owned-and-memory-stable.md)
- [Fibers architecture](fibers-architecture.md)
