# ASYNC-0025 - Use kqueue on BSD

Status: Accepted
Date: 2026-11-02

## Context

NetBSD, FreeBSD, and OpenBSD provide kqueue but differ from macOS in process-exit flags and file-to-socket transfer APIs.
The request lifecycle and caller-owned storage must remain consistent across the backends.

## Decision

Use the existing kqueue event-loop machinery for NetBSD, FreeBSD, and OpenBSD, selecting each system's native
process-exit flags. FreeBSD file sends use its native `sendfile` interface and retain any reported partial prefix. NetBSD
and OpenBSD file sends use a bounded stack buffer with positional reads and socket sends, re-reading an unsent suffix by
offset. They report `usedZeroCopy()` as false.

A buffered positional read can block the event-loop thread. As with other synchronous POSIX file-send paths, callers can
explicitly configure a supplied thread pool to move that work off the loop. This refines the blocking-work guidance in
ASYNC-0003 for this operation; it does not introduce an implicit worker or allocation.

## Consequences

The three tested BSDs share the public Async request and cancellation contracts, but the file-send mechanism and
performance can differ. NetBSD and OpenBSD file-send callbacks must not claim zero-copy. File-send progress, EOF, error,
and reactivation behavior need testing on every supported system; other BSD variants are not implied by this decision.

## Confirmation

Run Async lifecycle and contract tests on each BSD in Debug and Release. Exercise large file sends, interrupted calls,
short writes and backpressure, verify the observed prefix and zero-copy flag, and retain the caller-owned request and
thread-pool boundaries.
