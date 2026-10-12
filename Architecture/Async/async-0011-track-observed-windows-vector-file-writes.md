# ASYNC-0011 - Track Observed Windows Vector File Writes

Status: Accepted
Date: 2026-10-18

## Context

Borrowed file parity tests exposed Windows vector writes overwriting successive segments at one explicit offset and repeatedly submitting an empty segment on a native pipe. The backend credited requested lengths before asynchronous completion and did not inspect intermediate completion errors. A successful overlapped call also received both manual and IOCP completion handling.

## Decision

Use the existing totalBytesWritten cursor for observed completed bytes. Locate each remaining vector span from that cursor, skip empty entries, and advance explicit offsets by the completed prefix. Preserve the append sentinel for requests without an explicit offset. Submit one overlapped write at a time; inspect each completion before reactivation, preserving errors and completed-prefix counts. Reject zero progress on a nonempty span as FileWriteIncomplete.

For associated handles, consume the IOCP packet even when ReadFile or WriteFile returns success immediately. Association does not set FILE_SKIP_COMPLETION_PORT_ON_SUCCESS. Retain manual completion for immediate EOF errors that do not enqueue a packet. Initialize the immediate byte count before examining EOF.

## Consequences

Callbacks, Await, Fibers and AsyncExecution share the correction without a public layout change. Vector metadata remains borrowed and unchanged. Worker writes can still block; this decision does not introduce a new descriptor cursor or cancellation contract.

## Confirmation

Check explicit-offset scalar/vector contents, empty entries between nonempty pipe segments, actual byte counts, intermediate failures and reactivation without duplicate terminal callbacks. Run native and worker paths on Windows, followed by the default Async regression suites.

## Related

- [Microsoft IOCP notification modes](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setfilecompletionnotificationmodes)
- [Borrowed file options](../AsyncExecution/asyncexecution-0008-borrowed-file-options-and-complete-transfers.md)
