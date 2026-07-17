# FIBERS-0014 - Use Bounded Worker Idle Spinning Before Parking

Status: Accepted
Date: 2026-07-10

## Context

Workers that find no ready fiber currently park immediately through the generation-based wake event. This minimizes
idle CPU use, but an external producer or a near-simultaneous completion can pay an operating-system wake latency for
very short gaps. The worker-owned topology also requires idle probing not to reintroduce scheduler-global lock
contention.

## Decision

`FiberWorkerPoolOptions::idleSpinAttempts` is an explicit, bounded per-worker policy. Its default is 32 CPU-relax
checks. A value of zero disables spinning and parks immediately, which is appropriate for power-sensitive or sparse
workloads.

While idle, a worker performs at most that many checks of the scheduler's atomic ready-work count. Each check uses a
platform CPU-relax instruction and does not acquire the scheduler lock or allocate. A non-zero count retries normal
work discovery immediately. Once the bound is exhausted, the worker resets its local spin counter and follows the
existing wake-generation prepare, recheck, and OS-park sequence.

`idleSpinIterations`, `parkAttempts`, and `parkedWakeups` are exposed through `FiberWorkerDiagnostics`. They make the
latency-versus-idle-CPU tradeoff observable without tracing or allocating on the hot path.

## Consequences

The default can consume a small bounded amount of CPU before a worker parks, in exchange for avoiding some short-gap
parking latency. It cannot spin indefinitely and does not change task ordering, cancellation, queue capacity, or
ownership. Shutdown remains a wake-all operation; normal ready publication remains wake-one.

The policy does not make a performance claim on its own. It must be evaluated using the roadmap's quiet-machine
benchmark protocol, particularly for periodic external submissions and `FibersAsync` wakeups.

## Alternatives Considered

- Immediate parking only: retained as the explicit zero-spin option, but not the default because short producer gaps
  are common in a worker runtime.
- Unbounded busy waiting: rejected because it wastes a caller's CPU resources and gives no bounded behavior.
- Depend on `Threading` for a yield primitive: rejected because `Fibers` remains independent; the private platform
  CPU-relax operation is contained within the library implementation.
- Poll scheduler state under the global lock while spinning: rejected because it would add the contention this roadmap
  is intended to remove.

## Confirmation

A change preserves this decision when `idleSpinAttempts` remains a caller-selected finite bound, zero attempts reach
the existing park protocol without a spin iteration, idle probing does not acquire the scheduler-control lock or
allocate, diagnostics distinguish spin iterations from park attempts and wakeups, and publication or shutdown wakes a
parked worker without losing ready work.

## Related

- [FIBERS-0010 - Use worker-owned scheduling with bounded injection](fibers-0010-use-worker-owned-scheduling-with-bounded-injection.md)
- [Fibers active runtime roadmap](../../Documentation/Plans/FibersPlan.md)
