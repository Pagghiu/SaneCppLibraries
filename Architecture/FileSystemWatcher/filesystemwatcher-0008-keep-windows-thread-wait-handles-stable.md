# FILESYSTEMWATCHER-0008 - Keep Windows Thread Wait Handles Stable

Status: Accepted
Date: 2026-11-13

## Context

The Windows thread runner previously reset its registration count when adding a folder. Its wait
could also outlive a removed folder's event and overlapped storage.

## Decision

Reserve one wait handle for a control event. Adding or removing a watch signals that event and
waits for the worker to acknowledge its pause before changing the wait set. Keep the same worker
alive so Windows preserves the pending reads it issued on unaffected directories. The native limit allows 63
folders per thread runner. Return the existing capacity error when the wait set is full.

Cancel and drain a folder's overlapped read before releasing its handles or caller storage.
Close joins the worker before releasing the remaining watches. Return an error when a callback on
this worker tries to perform a lifecycle operation that would wait for itself. Callers serialize
lifecycle operations on an owner.

## Confirmation

Exercise two active roots, stop and re-register one, then close and reuse the runner.
Observe ordinary changes from each active root using native events to bound the test.

## Related

- [FILESYSTEMWATCHER-0007](filesystemwatcher-0007-report-watcher-coverage-loss.md)
- [WaitForMultipleObjects](https://learn.microsoft.com/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)
