# ASYNC-0014 - Isolate Windows Worker File Completions

Status: Accepted
Date: 2026-10-18

## Context

ForceThreadPool file reads and writes can target an overlapped descriptor already associated with IOCP. The worker previously submitted the request's native OVERLAPPED and waited for it. A controlled native-to-worker reuse regression stalled while waiting for that worker without polling IOCP. Associated status-block completion and worker completion therefore depended on the same loop that worker cancellation may need to join. Independently, ordinary IOCP notification would leave another packet referencing request storage after manual worker completion.

## Decision

Synchronous worker file I/O uses a local zero-initialized OVERLAPPED and a private manual-reset event. Set the valid event handle's low bit to suppress IOCP notification, as documented by GetQueuedCompletionStatus. Join pending I/O with GetOverlappedResult before publishing the existing manual worker completion. Close the untagged event handle on every return path. Event creation failures retain the operation's existing read/write error identity.

Keep the native event-loop path unchanged. Both scalar and vector writes and reads use the shared helper. Blocking descriptors retain their existing synchronous fallback. This requires no heap storage, public layout change, new library dependency or descriptor reassociation. Worker cancellation retains its join-before-terminal contract.

## Consequences

Worker operations no longer depend on the loop draining their native IOCP packet, and no such packet can outlive the worker's local storage. Each helper invocation creates a kernel event, including each vector element; this explicit fallback prioritizes portable ownership over minimizing worker-mode kernel calls.

## Confirmation

Reuse an associated native file-write request in forced worker mode, wait for worker completion without polling, dispatch its manual completion, then poll the kernel and verify exactly one callback. Cover vector writes and native-to-worker read reuse with content and byte-count checks. An independent monotonic deadline bounds regression failures. Run Windows Debug/Release default and sender suites, plus amalgamation checks.

## References

- [GetQueuedCompletionStatus event low-bit suppression](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus)
- [GetOverlappedResult event-based waiting](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult)
- [IOCP status-block completion rules](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
