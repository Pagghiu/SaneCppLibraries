# ASYNC-0002 - Split Event-Loop Running Into Submit, Poll, And Dispatch Phases

Status: Accepted
Date: 2026-07-04

## Context

Some applications can let `AsyncEventLoop` own the blocking run loop. Others, such as GUI apps or applications embedding another event loop, need to submit work, wait for kernel events, and dispatch callbacks at different times or on different threads.

## Decision

`AsyncEventLoop` exposes both convenience run modes and lower-level phases. `run`, `runOnce`, and `runNoWait` remain the simple integration points, while `submitRequests`, `blockingPoll`, and `dispatchCompletions` remain public building blocks for applications that need to integrate with another loop.

## Consequences

The event-loop interface is larger than a single `run` call, and callers using the split phases must preserve ordering and callback-thread expectations. In exchange, `Async` can cooperate with GUI loops, frame loops, and custom poll/dispatch strategies without introducing a separate adapter library.

## Confirmation

A change preserves this decision when split-phase APIs remain available, user callbacks run only during completion dispatch, `AsyncKernelEvents` remains caller-provided storage for polled events, and tests cover blocking-poll and dispatch behavior separately from the convenience run modes.

## Related

- [Async documentation: run modes](../../Documentation/Libraries/Async.md)
- [AsyncEventLoop](../../Libraries/Async/Async.h)
- [SC-0016 - Support layered adoption modes](../Global/sc-0016-support-layered-adoption-modes.md)
