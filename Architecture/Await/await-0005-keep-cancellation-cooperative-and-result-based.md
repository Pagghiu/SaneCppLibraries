# AWAIT-0005 - Keep Cancellation Cooperative And Result-Based

Status: Accepted
Date: 2026-07-04

## Context

`Await` cancellation must bridge coroutine control flow with the lower-level `AsyncRequest::stop` model. Cancellation may race with normal completion, and exceptions are not available for control flow.

## Decision

`Await` cancellation is cooperative and result-based. Cancelling a suspended task invokes the active awaiter's cancellation hook, usually stopping the underlying `AsyncRequest`. Cancelled awaits resume with `AwaitCancelledResult()`. If an operation has already completed, normal completion wins. Destroying an active task remains a programming error guarded by assertions rather than a recoverable runtime path.

## Consequences

Callers handle cancellation through ordinary `Result` checks and can distinguish cancellation with `AwaitIsCancelled`. Some cancellation requests cannot preempt already-running thread-pool work, so completion may still arrive first. Shutdown code must cancel or drain active tasks explicitly before storage goes out of scope.

## Confirmation

A change preserves this decision when cancellation does not throw exceptions, active awaiters expose explicit cancellation hooks, completion-wins behavior remains tested, task destruction asserts on active work, and docs describe cooperative cancellation limits.

## Related

- [SC-0006 - Use explicit result-based error propagation](../Global/sc-0006-use-explicit-result-based-error-propagation.md)
- [ASYNC-0001 - Keep AsyncRequest objects caller-owned and memory-stable](../Async/async-0001-keep-asyncrequest-objects-caller-owned-and-memory-stable.md)
- [Await documentation: cancellation](../../Documentation/Libraries/Await.md)
- [Await tests](../../Tests/Libraries/Await/AwaitTest.cpp)
