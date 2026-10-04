# FIBERS-0042 - Consume Wake Credits Before Condition Re-Parking

Status: Accepted
Date: 2026-10-04

## Context

Notifications advance the generation before locking the shared wake event. A waiter can enroll using that new
generation before the notifier takes the mutex. The notifier then wakes it without changing its observed generation.
The waiter re-enters the condition wait. Previously its pending notification credit was retained until the outer
wait returned, so a later scalar notification could suppress its OS signal despite a sleeping executor and ready work.

A gated probe of the actual event reproduces the stale-credit state and suppressed scalar signal. This is independent
of the distributed-counter quiescence protocol and does not establish the cause of older CI hangs.

## Decision

Consume an available pending credit after every OS condition-wait return, under the event mutex, before re-parking.
Apply the same protocol to POSIX and Windows. Remove the deferred once-per-outer-wait decrement.

Spurious condition returns can consume a credit and cause an extra subsequent signal; they cannot retain a consumed
notification credit that suppresses a required later signal. Generation remains the predicate, so spurious returns
do not imply runnable work or change the outer wait contract.

## Confirmation

Gate a broadcast between generation increment and mutex acquisition. Enroll a waiter on the new generation and let
it wake and re-park. A later scalar notification must issue a signal and release the waiter. Keep probe hooks outside
production source. Public persistent-wave tests exercise broadcast, re-parking and scalar submission with bounded
watchdogs. Validate both native and Fil-C runtimes and check wake/throughput diagnostics for excessive signaling.

## Related

- [FIBERS-0041](fibers-0041-confirm-distributed-job-quiescence-under-activation-gates.md)
