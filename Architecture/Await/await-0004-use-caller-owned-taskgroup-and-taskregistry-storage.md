# AWAIT-0004 - Use Caller-Owned TaskGroup And TaskRegistry Storage

Status: Accepted
Date: 2026-07-04

## Context

Coroutine code needs ways to spawn related child tasks and occasionally own detached/background tasks. A conventional task scheduler or heap-backed registry would hide task storage and shutdown responsibilities.

## Decision

`AwaitTaskGroup` uses caller-provided storage for child task pointers within a structured scope. `AwaitTaskRegistry` uses caller-provided fixed task slots for detached or background ownership. `waitAny` defaults to cancelling remaining active tasks, with explicit escape-hatch policies for callers that can prove remaining task storage outlives the wait.

## Consequences

Callers must size task storage and write explicit shutdown paths. Structured cases remain ergonomic through task groups, and detached cases remain possible through registries, without introducing hidden allocation or unowned background work.

## Confirmation

A change preserves this decision when task groups and registries continue to use caller-provided spans, default wait-any behavior cancels remaining tasks, escape-hatch policies are explicit, and tests cover full, empty, cancellation, wait-all, wait-any, and cleanup cases.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [Await documentation: task groups](../../Documentation/Libraries/Await.md)
- [Await documentation: detached tasks](../../Documentation/Libraries/Await.md)
- [AwaitTaskGroup and AwaitTaskRegistry](../../Libraries/Await/Await.h)
