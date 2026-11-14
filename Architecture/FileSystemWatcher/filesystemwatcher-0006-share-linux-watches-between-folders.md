# FILESYSTEMWATCHER-0006 - Own Linux watches by directory identity

Status: Accepted
Date: 2026-11-14

## Context

One inotify instance can return the same watch descriptor for overlapping roots. Removing it during
setup rollback or folder stop can lose another folder's coverage. Path-based registration can also
watch a replacement directory while recursive discovery still enumerates the original object.

## Decision

Open each descendant relative to its parent descriptor without following symlinks. Register the open
directory through `/proc/self/fd` and check that the name still resolves to that object after
registration. Skip entries that disappear during discovery. Fail setup on other discovery errors,
unavailable procfs, or a detected replacement.

Use bounded folder storage and record each registration before further work can fail. Detach a failed
folder and roll back all its registrations. Retry interrupted native removals and treat an already
removed watch as retired. Continue cleanup after a removal failure and return the cleanup error.
An open registration is shared until the last folder on that owner releases it. Apply this rule to
rollback and stop. Compare the current event with the same previous event for every listener.

## Consequences

The owner checks remaining folders before removing a native watch. It needs no extra reference table.
Directory changes can occur during discovery. Runtime invalidation and recovery follow
[the coverage contract](filesystemwatcher-0007-report-watcher-coverage-loss.md).

## Confirmation

Exercise overlapping roots and verify continued delivery after one root stops or rolls back.
Inject directory open, read, registration, and removal failures. Replace a descendant during
registration and verify that setup fails and acquired registrations are released.
