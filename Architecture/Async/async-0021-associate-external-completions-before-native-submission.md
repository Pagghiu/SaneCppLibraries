# ASYNC-0021 - Associate external completions before native submission

Status: Accepted
Date: 2026-11-04

## Context

On Windows, `AsyncExternalCompletion` allows a caller to start an external operation immediately after starting the
request. Its file handle was associated with the event loop's I/O completion port only when the loop later processed
its submission queue. A native operation completing in between could finish without posting a packet to the loop,
leaving the request active but unable to deliver its completion.

## Decision

Validate and associate a native Windows file handle synchronously in `AsyncExternalCompletion::start`, before returning
to the caller. The later submission phase must not detach and re-associate that handle. The internal null-handle wakeup
and the portable manual-completion path do not require a file-handle association and retain their existing behavior.

An invalid handle or a failed association is reported by `start` rather than deferred until event-loop submission.
This decision does not alter request lifetime, cancellation, or caller synchronization requirements.

## Confirmation

Complete an overlapped native read before the event loop processes the queued request, then verify that the loop
receives the completion and the correct byte. Exercise the watcher integration and existing external-completion and
native cancellation tests on Windows, as well as the common cross-platform suite.

## Related

- [ASYNC-0004 - Separate file readiness from external completion injection](async-0004-separate-file-readiness-from-external-completion-injection.md)
- [Microsoft: I/O completion ports](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
