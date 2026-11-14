# FILESYSTEMWATCHER-0007 - Report watcher coverage loss

Status: Accepted
Date: 2026-11-14

## Context

Native watch APIs can drop events, stop monitoring an invalidated root, or lose coverage for part of a tree.
An ordinary change notification gives callers no way to distinguish these conditions.

## Decision

Use `Change` for ordinary notifications, `RescanRequired` when the caller should reconcile lost or
incomplete observations, and `WatchStopped` when monitoring has ended. The two status notifications
use a root-level `AddRemoveRename` with an empty relative path so consumers of the original fields
still receive a change hint. `WatchStopped` reports the failure and preserves the identity of a foreign
runner error. Restoring recursive coverage can require stopping and registering the watch again.

- Linux broadcasts queue overflow to active folders as `RescanRequired`. Descendant invalidation,
  new directories, and an unrepresentable notification path request a rescan for the affected folder.
  Suppress invalidated subtrees. Stop a folder on root invalidation and the owner on fatal read, wait,
  or readiness failure. Classify invalidation per folder because a shared descriptor can identify
  different parts of overlapping watches. Walk the live folder list during broadcast so callbacks
  can remove watches.
- Apple routes FSEvents using canonical root paths and reports the caller's original spelling.
  Native drops and event ID wraps affect all active roots. Coalesced subtree and mount changes
  affect roots intersecting the native path by component. A root change stops the matching watch.
  A shared stream refresh requests a rescan because a new stream started at `SinceNow` cannot
  replay pending events on the old stream.
- Windows reports a zero-byte completion or `ERROR_NOTIFY_ENUM_DIR` as `RescanRequired` and
  rearms the read. Other completion failures and failed rearms stop that folder. A fatal worker wait
  stops all watches on the runner. A callback that stops or restarts a watch invalidates any pending
  rearm from the previous delivery.

Start the replacement Apple stream before retiring the old one. On setup failure, preserve
the old stream and detach the failed registration. Quiesce the handoff producer and reset its
generation across refresh. Copy callback and path data for delivery and keep the Async wake-up
active across refreshes. Close drains it. This updates the refresh-specific wake-up lifetime in
[FILESYSTEMWATCHER-0004](filesystemwatcher-0004-quiesce-apple-notification-handoffs-before-refresh.md).

## Consequences

Monitoring reports coverage changes the backend observes. It cannot promise lossless delivery.
After fatal Linux monitor failure, close and reinitialize the owner. Close a thread runner
from another thread when its callback is active. Async teardown can drive the caller's event
loop to drain pending cancellation and must follow the loop's serialization rules.

## Confirmation

Exercise overflow or drop, root invalidation and failed rearm on the applicable backends. Verify
terminal watches stop delivering, unaffected roots keep delivering and restored watches can be
reused. Exercise Apple callback-driven refresh across overlapping roots and Windows stop in a
completion callback.
