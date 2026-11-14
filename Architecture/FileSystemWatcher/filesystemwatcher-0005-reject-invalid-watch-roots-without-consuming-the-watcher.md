# FILESYSTEMWATCHER-0005 - Reject invalid watch roots without consuming the watcher

Status: Accepted
Date: 2026-11-03

## Context

The public watch operation describes a directory, but FSEvents accepted a missing path while the Linux and Windows
backends rejected it. On the latter platforms, early native setup failures left the `FolderWatcher` linked to its owner,
so a subsequent watch using the same object failed as already watching. A regular file also reached Windows directory
change submission and left partially initialized resources behind.

## Decision

Require an existing directory as the root of a watch. If the root cannot be opened or is not a directory, return
`WatchSetupFailed` and leave the `FolderWatcher` available for another watch. Check the root before creating the Apple
event stream, open the Linux directory before registering its root watch, and validate the Windows handle before setting
up notifications. Preserve native error context at the backend stage that failed.

This decision covers root validation and registration failures only. It does not claim rollback for every later
recursive setup, thread-start, or asynchronous completion failure. A path can also change while setup is in progress.

## Confirmation

Use shared thread-runner and event-loop tests on macOS, Linux and Windows to reject invalid roots, then watch a valid
directory with the same object. Inject a Linux root-registration failure and verify the opened root descriptor is closed
and the watcher can be reused. On Windows, check process-handle ownership across repeated regular-file rejections.

## Related

- [FILESYSTEMWATCHER-0002 - Expose coarse portable event classes](filesystemwatcher-0002-expose-coarse-portable-event-classes-over-native-watch-apis.md)
- [FILESYSTEMWATCHER-0003 - Structured result errors](filesystemwatcher-0003-structured-result-errors.md)
