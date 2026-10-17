# ASYNCEXECUTION-0009 - Borrowed File Send

Status: Accepted
Date: 2026-10-18

## Context

Await and AsyncFibers expose AsyncFileSend's explicit file range, transferred-byte count and transfer mechanism. Sender composition needs the same information without caller-owned native request plumbing. File send has native support even on Windows, where ordinary file reads/writes normally need a pool.

## Decision

Expose fileSend(file, socket, options, optionalProgress), emitting AsyncExecutionFileSendResult with bytesTransferred, usedZeroCopy and complete. Options carry offset, length, the existing pipe-size hint, an optional pool and NativePreferred/ForceThreadPool mode. Resources remain borrowed through terminal completion. Paths and whole-file buffers belong to the separate owning conveniences.

NativePreferred uses the backend file-send operation; supplying a pool does not move this native-capable operation onto that pool. ForceThreadPool explicitly selects Async's worker implementation and requires a pool. Windows worker fallback requires a blocking file and uses copied socket I/O; its seek mutates the descriptor cursor, so independent concurrent users require separate descriptors. Native platform failures retain their Result identity. No automatic copied-I/O fallback or stronger zero-copy guarantee is invented.

Validate nonnegative signed-64-bit ranges before submission. Bound this single native operation to 2,147,483,646 bytes, the common TransmitFile limit. Larger transfers can compose consecutive explicit ranges. Length zero retains Async's EmptyTransfer error rather than meaning whole-file transfer.

Initialize optional progress lazily on the owner loop before checking pre-stop. Observe native callback results before selecting value/error/stopped so errors retain their known transferred prefix. Cancellation waits for quiescence; if native cancellation supplies no completion count, progress does not guess kernel-side bytes. Complete is true only for a successful full requested range. Keep all non-template logic in AsyncExecution.cpp and use the provider's algorithms for composition.

## Consequences

Sender code can join file sending with peer receiving using when_all. Borrowed operations stay comparable to the other adapters without depending on them. Native file-send preference is more explicit than Await's current unconditional attachment of a supplied pool. A later complete-file convenience can compose ranges without weakening this primitive's limits.

## Confirmation

Check native and forced-pool transfers, nonzero offsets, data across splice/chunk boundaries, known progress on early EOF, invalid ranges/descriptors, missing forced pools, pre-stopped initialization and terminal cleanup. Validate native progress independently of an available but busy pool before closing the file-group milestone.

## Related

- [Async EOF and splice-stage contract](../Async/async-0012-finish-file-send-on-zero-progress.md)
- [Microsoft TransmitFile limits](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-transmitfile)
