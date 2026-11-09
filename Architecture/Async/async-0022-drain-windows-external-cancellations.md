# ASYNC-0022 - Drain Windows External Cancellations

Status: Accepted
Date: 2026-11-12

## Context

A Windows external request may have submitted native I/O before the loop activates it.
An operation can also finish before cancellation and leave a packet in IOCP or in a
batch already collected by an enclosing dispatch. The packet refers to caller-owned
`OVERLAPPED` storage.

## Decision

Cancel pending native external operations regardless of whether the loop has activated the
request. Keep the request and its storage live until the packet is consumed, including when
`CancelIoEx` reports that the operation has already finished. A nested cancellation can
consume an undispatched packet from an enclosing dispatch batch before releasing storage.
The Windows event loop keeps one pointer to the active batch chain in its opaque storage.

The Windows watcher keeps its directory handle open through cancellation and releases the
completed request from the per-folder stop call.

## Confirmation

Stop a read before its first loop submission, stop after native completion while the packet
is still queued, and stop from a callback after the packet has been collected. Verify that
normal completion callbacks are suppressed and that the watcher can immediately reuse its storage.

## Related

- [ASYNC-0021](async-0021-associate-external-completions-before-native-submission.md)
- [Microsoft CancelIoEx documentation](https://learn.microsoft.com/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)
