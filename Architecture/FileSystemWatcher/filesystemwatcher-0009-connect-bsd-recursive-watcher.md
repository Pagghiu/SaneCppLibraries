# FILESYSTEMWATCHER-0009 - Reconcile BSD directory watches with kqueue

Status: Accepted
Date: 2026-11-14

## Context

NetBSD and OpenBSD report vnode events for open objects. Recursive coverage needs to track
a changing directory tree and stale events can outlive the descriptors that originally
generated them. The watcher must remain bounded and preserve its two delivery modes.

## Decision

Use one kqueue per owner and a bounded inventory per folder. Register directories before
scanning them. Read native records into bounded scratch storage, checking their sizes and
name terminators. Open descendants relative to the parent descriptor and verify type and
identity. Each watched regular file or directory consumes a descriptor. Identify queued
events by nonzero registration tokens shared across the queue. Tokens are never reused
while stale events can be delivered, even if a descriptor or inventory slot is reused.

On structural hints, match records by identity, retire missing subtrees, then discover and
verify replacements. Preserve the descriptor and token of a moved subtree when its old
name no longer identifies the object and the move matches a unique directly watched record.
For a regular file, require a single hard link. Rebuild ambiguous registrations. Skip
descendants that disappear during lookup or opening and retire entries absent from the
verified tree. Retry one identity mismatch and report `RescanRequired` if coverage is
restored. Deliver an ordinary root hint for ambiguous changes and report `RescanRequired`
if a changed path cannot be represented.

Report `CoverageLimited` at startup and when newly discovered passive paths lack direct
coverage. Do not follow symlinks or register special files. Use the root-level operation for
coverage statuses, following [the portable contract](filesystemwatcher-0007-report-watcher-coverage-loss.md).
Root loss, vnode revocation, failed reconciliation, and fatal queue or readiness failures
stop the shared monitor. Close and reinitialize the owner before adding watches again.

Use the queue's user event for thread wakeup and the existing Async template bridge for
queue readiness. Serialize inventory and folder-list changes. Copy callback and path data
and release the mutex before calling user code. External stop waits for an active thread
callback while self-stop can return from the callback. A worker cannot join itself. Async
callbacks may stop a watch or close an owner, with the caller serializing loop operations.

## Consequences

The default bounds are 128 records including the root and 4 KiB of names. The caller can
lend disjoint record and name buffers until stop. Filesystem block size and OS descriptor
limits can constrain discovery. Setup failures roll back registrations and descriptors,
and the watch reports a cleanup failure if cleanup fails. Scans cannot guarantee a
lossless history of directory changes.

## Confirmation

Run shared threaded and Async tests on NetBSD and OpenBSD and verify descriptor reuse,
nested changes, root loss, callback shutdown, rescan hints, and recovery. Exercise bounded
storage and native scan, registration, and cleanup failure paths.

## FreeBSD directory notifications

FreeBSD uses its native inotify API with the shared Linux directory inventory.
Register each open directory with `inotify_add_watch_at`; regular-file events
arrive through the directory registration. Caller storage extends the maximum
number of directories. A small kqueue observes the inotify descriptor and a user
event, allowing both delivery modes to report passive entries at registration
even when the directory itself is idle. The shared backend retires departed
subtrees and reports `RescanRequired` after discovering arrivals. A failed
registration stops that folder. Other folders on the owner can keep monitoring.

FreeBSD still reports `CoverageLimited` for symlinks and special files.
Directory watches can report some changes to those entries, such as symlink
metadata updates, but the contract allows partial coverage. Confirm the
caller-storage and descriptor-limit cases through the existing watcher suite.
