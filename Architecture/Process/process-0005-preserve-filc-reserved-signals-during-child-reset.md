# PROCESS-0005 - Preserve Fil-C Reserved Signals During Child Reset

Status: Accepted
Date: 2026-10-06

## Context

The native POSIX launch path resets dispositions and clears the signal mask before exec. Fil-C formerly
skipped both operations because resetting a runtime-owned signal returns ENOSYS. Exec does not itself
clear blocked signals or reset ignored dispositions, so that bypass changes observable child semantics.

## Decision

Use the same reset sequence for native and Fil-C executable launches. Under Fil-C, skip only signals
reported by zis_unsafe_signal_for_handlers, plus the universally unchangeable SIGKILL and SIGSTOP.
Do not hard-code a runtime-reserved signal list: rootless and glibc distributions differ. Clear the child
mask through pthread_sigmask after disposition reset. Preserve existing failure propagation for unexpected
sigaction errors and mask errors; do not silently ignore ENOSYS for arbitrary signals.

## Consequences

Supported signals have consistent launch behavior and the parent's state remains unchanged. Fil-C keeps
ownership of its internal/safety signals. This requires a runtime exposing the signal-support predicate
(present in the currently pinned 0.685). ProcessFork snapshot semantics are unchanged.

## Confirmation

ProcessTest launches a child from a parent with SIGUSR1 ignored and blocked. The child checks default
disposition and an unblocked mask; the parent verifies its state is unchanged and restores its original
state. The regression fails with the old Fil-C bypass and passes with selective reset. Validate both
rootless and glibc distributions and native POSIX platforms.

## Related

- [PROCESS-0003](process-0003-keep-processfork-as-a-caveated-snapshot-primitive.md)
- [Process documentation](../../Documentation/Libraries/Process.md)
- [Fil-C signal API](https://github.com/pizlonator/fil-c/blob/v0.685/filc/include/stdfil.h)
