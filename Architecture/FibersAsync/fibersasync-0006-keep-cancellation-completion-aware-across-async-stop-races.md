# FIBERSASYNC-0006 - Keep Cancellation Completion-Aware Across Async Stop Races

Status: Accepted
Date: 2026-07-07

## Context

`FiberAsyncIO` operations bridge fiber waits to lower-level `AsyncRequest` start, completion, and stop paths. Cancellation
can race with request start, owner-thread command execution, backend completion, and failed starts. Incorrect accounting
can leave fibers waiting forever, reuse stack-local command state too early, or complete an operation twice.

## Decision

`FibersAsync` cancellation is cooperative and completion-aware. Before creating pending-operation state or starting an
`AsyncRequest`, the bridge requires a currently running fiber of its scheduler so all stack-local request and callback
state can remain alive across suspension. Once an operation starts or a start command is queued, the bridge must balance
operation counters exactly once, acknowledge cancellation-before-start on the owner thread, and treat backend completion
and stop completion as racing outcomes where one final result wins.

## Consequences

Cancellation paths are more explicit than a simple "stop and forget" API, but they preserve bounded command storage and
fiber wake correctness. Failed starts, queued starts, and stop requests must all release pending-operation state without
touching producer stack state after it may have gone out of scope.

## Confirmation

A change preserves this decision when start/stop/complete interleavings cannot leak pending operation counters, queued
commands never retain invalid producer stack state, cancellation-before-start is handled by the owner thread, and tests
cover rejection outside a fiber, cancel-after-submit, cancel-before-start, failed-start, completion-wins, and
post-exhaustion reuse. The bridge must not be destroyed until both pending-operation accounting and queued command state
are empty.

## Related

- [FIBERS-0006 - Keep cancellation cooperative and wake-based](../Fibers/fibers-0006-keep-cancellation-cooperative-and-wake-based.md)
- [FIBERSASYNC-0002 - Keep AsyncEventLoop owner-thread affine with bounded command posting](fibersasync-0002-keep-asynceventloop-owner-thread-affine-with-bounded-command-posting.md)
- [FibersAsync architecture](fibersasync-architecture.md)
