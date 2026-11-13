# ASYNC-0020 - Recheck Windows timer deadlines after poll

Status: Accepted
Date: 2026-11-03

## Context

`runOnce` drives the next timer through a Windows completion-port wait. A native wait can report a timeout before the
monotonic deadline; Windows documents that short waits can be affected by
[clock resolution](https://learn.microsoft.com/en-us/windows/win32/sync/wait-functions). Returning from that poll
without an I/O completion or an expired timer can leave `runOnce` without the expected timer progress.

## Decision

After a timed completion-port wait reports `WAIT_TIMEOUT`, refresh the monotonic clock. If the next timer has not yet
expired, wait for the remaining interval. Return immediately for an I/O completion, and keep nonblocking polls and
interrupted steps nonblocking. Cap a long finite wait at the largest finite Windows wait rather than allowing conversion
to wrap or become `INFINITE`.

## Consequences

An early native timeout alone does not cause `runOnce` to return before a pending timer is due. This does not attribute
any particular intermittent test failure to clock resolution, and does not change timer ownership or callback lifetime.

## Confirmation

In AsyncContractTest, drive the Windows polling policy with a simulated early timeout followed by a timeout at the
deadline or an I/O completion. Check the remaining wait, the finite cap for long timers, and the single zero-timeout
poll in nonblocking mode. The shared callback and wake-up tests exercise dispatch through the real event loop.

## Related

- [ASYNC-0002 - Split event-loop running into submit, poll, and dispatch phases](async-0002-split-event-loop-running-into-submit-poll-and-dispatch-phases.md)
- [ASYNC-0019 - Do not block after owner interrupt](async-0019-do-not-block-after-owner-interrupt.md)
