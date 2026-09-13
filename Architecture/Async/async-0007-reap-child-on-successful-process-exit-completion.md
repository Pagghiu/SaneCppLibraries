# ASYNC-0007 - Reap Child on Successful Process Exit Completion

Status: Accepted
Date: 2026-09-24

## Context

Linux `AsyncProcessExit` calls `waitpid` before a successful callback. macOS previously returned the status from a
kqueue `NOTE_EXIT` event without waiting, leaving a zombie child. The public callback looked like one portable exit
operation, and callers such as Await and the repository build tools consumed it as the final process result.

## Decision

Successful POSIX `AsyncProcessExit` completion waits for and reaps exactly one child before invoking its callback.
The macOS kqueue backend calls `waitpid` for both ordinary `NOTE_EXIT` and early `ESRCH` completion. Linux retains its
existing wait behavior. The returned exit code is the child's normal exit code, or `-1` when it exited by signal.

The caller owns the child after a failed start or a stopped request that did not complete. It must arrange any needed
termination and wait. A child must have only one exit request; concurrent waiters or a separate `Process::waitForExitSync`
can consume the status first. Windows completion reports status from the native process handle and does not transfer
handle ownership.

## Consequences

macOS callers that previously waited after the exit callback must stop doing so. The API has one POSIX success
contract, while cancellation and setup failures still require explicit caller cleanup. A callback error from a failed
OS wait does not promise that this request reaped the child.

## Confirmation

The Async process exit test checks normal and nonzero callbacks and verifies `waitpid` reports `ECHILD` afterward on
POSIX. The stop-before-completion test verifies the caller can still wait after cancellation. Run these tests on both
macOS and Linux, including Linux epoll and io_uring when available.

## Related

- [AsyncProcessExit](../../Libraries/Async/Async.h)
- [Async documentation](../../Documentation/Libraries/Async.md)
