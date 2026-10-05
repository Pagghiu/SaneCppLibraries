# FIBERS-0043 - Relax Contended Job Activation Gate Waits

Status: Accepted
Date: 2026-10-05

## Context

Distributed quiescence confirmation serializes through the global activation gate. Persistent short-job waves
can make several executors and an idle observer contend on that gate. Retrying an acquire CAS in a tight loop
repeatedly requests exclusive cache-line ownership while another thread still holds the gate.

Controlled accounting-only and wake-credit-only variants isolate this cost from the notification-credit repair.
Separate diagnostic runs identify global-gate failed acquisitions, rather than worker-gate contention or idle
observer relays, as a concrete source of excessive atomic traffic.

## Decision

Use a dedicated wait strategy only for job accounting activation gates:

- Attempt acquire CAS immediately, preserving the uncontended path.
- On POSIX, poll a held gate with relaxed atomic loads and CPU-relax; retry acquire CAS after it becomes free.
- On Windows, retain interlocked CAS acquisition with CPU-relax after failure. Do not pretend the existing
  interlocked load helper is a read-only poll, and do not replace atomic polling with an ordinary volatile read.
- Keep release unlock, activation ordering, global serialization and fully gated zero confirmation unchanged.
- Leave other scheduler locks and public layout unchanged.

Polling and CPU-relax never grant ownership or supply the confirmation synchronization. Only successful acquire
CAS grants the lock; release unlock still supplies the happens-before chain required by final completers.
The gates remain unfair spinlocks, without a starvation-freedom guarantee.

## Confirmation

Compare uninstrumented interleaved throughput and complete-wave latency against the retained correctness fix,
including small-worker and mixed-core cases. Diagnostic retry/poll counts are attribution evidence, not throughput
measurements. Preserve bounded idle/ready/wake probes and the independent completion/snapshot models; validate
native and both Fil-C runtime distributions plus hosted Windows. Do not claim cross-platform performance parity
from one host's timings.

## Related

- [FIBERS-0041](fibers-0041-confirm-distributed-job-quiescence-under-activation-gates.md)
- [FIBERS-0042](fibers-0042-consume-wake-credits-before-condition-reparking.md)
