# ASYNCEXECUTION-0011 - Borrowed Process and Signal Senders

Status: Accepted
Date: 2026-10-18

## Context

Async, Await and AsyncFibers already expose child-process waits and signal deliveries. Sender composition must preserve native result identity and process ownership while expressing one completion. A successful POSIX wait reaps the child; cancellation and composition may discard a successful sender value after that native side effect.

## Decision

Add processExit(FileDescriptor::Handle, optional progress) without a public dependency on Process. Borrow the child handle through terminal completion and emit AsyncExecutionProcessExitResult {exitStatus, complete}. A nonzero exit status is a value; native watch/wait failures use the error channel. Only one wait may observe a child, and successful POSIX completion follows ASYNC-0007 and reaps it. Never kill a child as part of stopping its wait.

Record successful native completion in optional caller-owned progress before selecting the sender terminal channel. Reset progress lazily at start. complete remains false if this watch stops before native completion; the caller still owns waiting for that child. If complete is true, POSIX reaping already occurred even when stop or a failing enclosing composition suppresses the value. Do not wait for that child again. This observer remains borrowed through quiescence; do not alias it with a root output whose preservation matters. A graph without the observer must account for native wait side effects if its values can be discarded.

Add signal(number, options) emitting AsyncExecutionSignalResult {signalNumber, deliveryCount}. Copy options; complete on one native delivery without reactivation. Current mode/coalesce fields retain Async's reserved/backend-dependent policy and do not create an automatically persistent sender. Native unsupported/invalid signal errors remain unchanged. Use one watcher per POSIX process signal for portable behavior and fan out above it. Windows supports the existing console-control mapping only; delivery requires an appropriate console/process group. Do not promise portable observation of thread-directed POSIX signals.

Use existing owner-loop publication, stop registration, quiescence and error precedence. Keep non-template methods in AsyncExecution.cpp. No new library dependency or custom execution algorithm is introduced.

## Confirmation

Test child exits 0/7 as values, invalid handles, prestop/output preservation, active cancellation then caller wait, and a deterministic stop after native reaping with progress retained. Test process-directed POSIX signal delivery, invalid/unsupported signal identity, prestop and active cancellation. Windows console delivery uses a ready child in a new process group and CTRL_BREAK; bound event/process waits and clean up only that fixture child. Run focused, default/dedicated Debug/Release/platform and integration gates before AE-044 is checked.

## References

- [ASYNC-0007](../Async/async-0007-reap-child-on-successful-process-exit-completion.md)
- [Apple EVFILT_SIGNAL process notification](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/kqueue.2.html)
- [Microsoft console process-group control delivery](https://learn.microsoft.com/en-us/windows/console/generateconsolectrlevent)
