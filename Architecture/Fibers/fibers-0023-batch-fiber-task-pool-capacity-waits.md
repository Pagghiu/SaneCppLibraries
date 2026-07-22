# FIBERS-0023 - Batch Fiber Task Pool Capacity Waits

Status: Accepted
Date: 2026-07-22

## Context

`FiberTaskPool::waitForAvailableTask()` wakes a bounded producer as soon as one task/stack pair can be reused. This is
the correct minimum-progress contract, but a sustained producer commonly refills that one slot and immediately waits
again. The resulting suspend/resume cycle adds synchronization and completion coordination for every small batch.

The caller already knows its desired refill granularity and the pool has a fixed capacity. The runtime does not need
allocation, a future, or an unbounded producer queue to express that information.

## Decision

`FiberTaskPool::waitForAvailableTasks(scheduler, minimumAvailable)` suspends until at least the requested number of
task/stack pairs are reusable. The minimum is explicit, must be nonzero, and cannot exceed `capacity()`. The existing
`waitForSpawnCapacity()` and `waitForAvailableTask()` methods remain one-slot aliases.

Fixed-storage pools retain the existing stack-local waiter node and add the threshold to that node. Completion checks
the FIFO head and publishes its counter only after the threshold is satisfied. A larger head request therefore retains
arrival-order fairness and may delay a smaller request queued behind it. Cancellation removes an unnotified waiter and
hands notification to the next eligible waiter through the existing cooperative path.

Class-backed pools provide the same result by waiting on their bounded task and stack classes until their shared
available count reaches the requested threshold. No hidden allocation or new inter-library dependency is introduced.

## Consequences

Callers choose batching explicitly; the library does not guess task cost or reserve slots after the wait returns.
Another producer can consume capacity before the caller spawns, so the normal spawn result remains authoritative.
Fixed `FiberTaskPool` slot acquisition is still a single-producer or externally serialized operation.

In a quiet macOS ARM64 Release comparison, the sustained one-million-job benchmark waited for 64 of 512 slots at a
time. Five measured samples held median throughput within 1.1% of the one-slot baseline while reducing synchronization
and completion lock acquisitions by approximately 82%, satisfying the targeted-cost acceptance gate.

## Alternatives Considered

- Increase pool capacity only: useful caller tuning, but it consumes more caller-owned task and stack storage and does
  not express refill policy.
- Wake every waiter after each completion: rejected because it creates a herd and abandons FIFO fairness.
- Reserve the requested slots for the waking producer: rejected because it would add ownership state and alter the
  existing spawn-capacity contract.
- Infer a batch size inside the runtime: rejected because queue depth does not reveal task cost or caller latency needs.

## Confirmation

A change preserves this decision when invalid thresholds fail before suspension, fixed and class-backed pools resume
only after the requested count is available, one-slot APIs remain compatible, cancellation safely unlinks waiters,
pool reuse remains allocation-free, and callers still handle a subsequent spawn failure if capacity races.

## Related

- [FIBERS-0007 - Model spawn backpressure as explicit capacity waiting](fibers-0007-model-spawn-backpressure-as-explicit-capacity-waiting.md)
- [FIBERS-0022 - Coalesce redundant worker wake signals](fibers-0022-coalesce-redundant-worker-wake-signals.md)
- [Fibers architecture](fibers-architecture.md)
- [Fibers documentation](../../Documentation/Libraries/Fibers.md)
