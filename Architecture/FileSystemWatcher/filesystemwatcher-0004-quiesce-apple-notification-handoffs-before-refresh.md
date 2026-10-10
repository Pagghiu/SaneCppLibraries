# FILESYSTEMWATCHER-0004 - Quiesce Apple Notification Handoffs Before Refresh

Status: Accepted
Date: 2026-10-10

## Context

The Apple backend delivers FSEvents through a worker thread that waits for an Async wake-up callback. Changing the watched paths synchronously refreshes the worker's stream. Waiting for that refresh while the worker waits for the caller to dispatch a notification creates a circular wait. A timeout in the caller's event loop cannot resolve it.

## Decision

Treat stream refresh and close as notification handoff boundaries. Suppress notifications during refresh, release the worker's pending handoff, and wait for the worker to reach the boundary. Cancel and drain the old Async wake-up before restarting it after refresh; close drains it without restarting. Consume any unused handoff signal before resuming notifications.

Reactivate the wake-up before invoking the user's notification callback so callback-driven shutdown can cancel the request. The bridge acknowledges delivery itself, skipping the acknowledgement when a callback has crossed a refresh/close generation boundary, so no stale signal leaks into a restarted watch. Keep this orchestration in the template bridge to preserve the library's independence from Async.

## Consequences

Stopping or closing a watcher does not depend on the caller dispatching a previously queued notification. The bridge request reaches Free before its storage can be destroyed. Notifications overlapping a stream refresh can be discarded; callers must already reconcile coarse, coalesced filesystem notifications rather than treating them as a transaction log.

Draining cancellation can drive the caller's event loop. Watch-list mutations remain owner-thread operations and must not run while another thread is polling the same Async loop.

## Confirmation

`FileSystemWatcherAsyncTest::AsyncEventLoop stop with pending notification` uses real FSEvents to stop a watcher while dispatch is paused, rewatch and receive a new notification, stop from inside a callback and restart, then close with another notification pending. Stopped notifications must not reach the callback, and loop teardown must finish.

## Related

- [Template bridge](filesystemwatcher-0001-keep-async-integration-as-a-template-bridge.md)
- [Portable event classes](filesystemwatcher-0002-expose-coarse-portable-event-classes-over-native-watch-apis.md)
