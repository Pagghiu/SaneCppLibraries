# FIBERS-0041 - Confirm Distributed Job Quiescence Under Activation Gates

Status: Accepted
Date: 2026-10-04

## Context

A distributed scan can read global zero, then miss an existing worker job after a new global job is accepted and the
worker job completes. It reports false idle despite continuously active work. Adding destination accounting before
subtracting source accounting protects an individual transfer, but does not make a multi-domain scan atomic.

Independent final decrements followed by acquire scans also permit both completers to observe the other's old count
under the C++ memory model. Neither broadcasts to an already parked idle observer. Stronger current ARM/x86 lowering
does not supply a portable source-level guarantee. A shared per-job completion counter would undo distributed accounting.

## Decision

Keep distributed ready and active counters, with one activation gate per scheduler/worker domain.

- Additions to positive counts use a positive-only compare/exchange. If a decrement reaches zero first, the CAS fails.
- Every zero-to-positive transition holds the domain gate. Charge active before ready, and finish accounting before
  publishing runnable work. Skip zero-sized additions during batch claims.
- Positive aggregate observations retain the bounded unlocked scan. Candidate-zero observations lock the global gate,
  then worker gates in ascending order, and rescan directly. Unlock in reverse order.
- A count observed as zero cannot reactivate while its gate is held. Thus an all-zero confirmation has an observation
  point at its final read, without claiming that positive totals are simultaneous snapshots.
- Every active decrement that empties its domain performs locked confirmation, without an unlocked early rejection.
  These confirmations serialize through the global gate: the last confirmer imports earlier decrements through
  release/acquire lock synchronization and broadcasts after releasing all gates.
- Under the global gate, a positive rescan may return without locking worker gates. Only a candidate zero acquires
  all worker gates and rescans every counter again. This reduces positive-confirmation cache-line writes without
  bypassing the global synchronization required by the last completer.
- Confirmation never takes the queue lock. Publishers release each activation gate before acquiring another or
  notifying, invoking callbacks, or reclaiming storage. No gate covers job execution.

## Consequences

No allocation, registry growth, runtime-specific fence, or globally contended per-job completion RMW is introduced.
Ordinary completion and ready decrements remain distributed. Activation, zero observation and final-domain completion
pay a short gated cost; positive additions may retry CAS. Measure throughput against the existing worker matrix and
persistent-wave benchmark. Unfair spinlocks do not promise starvation freedom under unending submission.

`waitIdle()` retains an actual idle observation point, not a closed submission barrier. Later submissions may execute
before it returns. Producers still need coordination before storage release; pool teardown must not overlap observers.

## Confirmation

Preserve bounded before/after scheduler probes for continuously active mixed snapshots and ready-domain reactivation.
Model-check independent final decrements and gated zero confirmation; distinguish language-model violations from
actual target failures. Public tests cover continuous external handoff at one/two/four workers and parked observers
released by four final domains. Run native and Fil-C regressions plus baseline/fixed throughput samples.

## Related

- [FIBERS-0027](fibers-0027-distribute-stackless-active-job-accounting.md)
- [FIBERS-0030](fibers-0030-distribute-stackless-ready-job-accounting.md)
- [FIBERS-0028](fibers-0028-keep-job-worker-pools-alive-between-waves.md)
