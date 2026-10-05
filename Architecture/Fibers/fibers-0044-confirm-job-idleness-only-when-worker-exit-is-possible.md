# FIBERS-0044 - Confirm Job Idleness Only When Worker Exit Is Possible

Status: Accepted
Date: 2026-10-05

## Context

A persistent job worker checked active-job counts after every unsuccessful execution attempt, including
idle spins. Without a stop request, the result could not make that worker exit. A zero count nevertheless
required serialized activation-gate confirmation. Diagnostic counts show those unused observations can
substantially outnumber final-domain completion confirmations.

## Decision

Check whether the pool is nonpersistent or stopping before querying active jobs in the initial worker-loop
exit condition. Both cases still require confirmed active zero before exit. Persistent, nonstopping workers
continue directly to their unchanged idle-spin or prepare-to-park path.

Keep the post-preparation stop/active/ready predicate, preparation fences, wake generation and notification
credits unchanged. Keep every final-domain completion confirmation and public count-query guarantee.
The removed query was not an unconditional synchronization barrier: positive counts could return without
acquiring an activation gate.

## Consequences

Persistent idle spinning avoids an unused zero observation without changing job claim size, fairness policy,
worker migration or stack restrictions. A concurrent stop not observed by the first condition is still handled
by the post-preparation predicate or wake protocol. This does not establish the cause of any historical hang.

## Confirmation

Exercise immediate stop, parked stop and stop while draining with both zero and nonzero idle-spin attempts.
Retain bounded stop-before-prepare probes on native Linux and Fil-C, plus parked publication waves and
nonpersistent automatic exit coverage. Compare uninstrumented throughput separately from diagnostic counts;
do not claim universal speedup from one guest's measurements.

## Related

- [FIBERS-0041](fibers-0041-confirm-distributed-job-quiescence-under-activation-gates.md)
- [FIBERS-0042](fibers-0042-consume-wake-credits-before-condition-reparking.md)
- [FIBERS-0043](fibers-0043-relax-contended-job-activation-gate-waits.md)
