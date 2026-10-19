# ASYNCEXECUTION-0018 - Coalescing Notification

Status: Accepted
Date: 2026-10-19

## Context

A wake-up sender needs a stable producer-facing address without exposing connected operation storage. SC::Async already supplies a thread-safe wake-up primitive; AsyncExecution uses one for its owner dispatcher. Requiring applications to construct and synchronize another native request would defeat the convenience layer.

## Decision

Add a nonmovable AsyncExecutionNotification bound to a borrowing context. context.wakeUp(notification) is a lazy, one-shot sender<void>; notification.notify() is thread-safe and returns Result. Notifications coalesce into one pending bit, including notifications before a wait. Only one active waiter is allowed; another receives NotificationAlreadyWaiting. A notification from another context receives NotificationContextMismatch.

Use the existing context command dispatcher and its AsyncLoopWakeUp, rather than one native request per notification. Producer publication, waiter attachment and detachment are protected by the notification mutex. Always acquire that mutex before the context queue mutex. Dispatch detaches the waiter and consumes the pending bit before terminal delivery. Cancellation removes a queued notification before delivering stopped, leaving an unconsumed pending bit for the next wait. If dispatch selected a notification before observing stop, that selected bit is consumed even when stop wins terminal delivery. Prestop does not attach or consume a bit.

All terminal delivery occurs on the context owner thread. Connected storage may be destroyed during delivery. Stop registration is removed and both commands are unlinked first. A notify failure retains the pending bit; the caller must handle the error and may retry. Successful notify means publication, not callback completion. No detached work, allocation or worker is added.

The notification and context outlive their active wait and any concurrent notify calls. Join producer threads before destroying them or closing the context. An idle notification may be destroyed; its destructor asserts no attached waiter or queued command. The context's existing outstanding count prevents closing over a waiting graph.

This is an SC coalescing wake-up extension, not a standard execution algorithm or an arbitrary external-operation cancellation bridge. External completion still needs a separate producer acknowledgement contract.

## Confirmation

Test notify-before-wait, coalescing, rearming, actual cross-thread notification, overlapping waits, context mismatch, prestop, active cross-thread stop and subsequent reuse. Free connected storage inside value/error/stopped callbacks. Verify context close rejects an active waiter. Run dedicated Debug/Release suites on available platforms and preserve disabled-provider, unity, amalgamation and separate-TU boundaries.
