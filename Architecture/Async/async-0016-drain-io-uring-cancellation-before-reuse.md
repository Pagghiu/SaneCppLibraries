# ASYNC-0016 - Drain io_uring Cancellation Before Reuse

Status: Accepted
Date: 2026-10-18

## Context

The split submit/poll/dispatch interface lets an owner collect a native failure, stop its request, then dispatch that batch. Linux io_uring filtered successful completions for cancelling requests but interpreted negative results first, invoking the normal callback while cancellation owned the request. Independently, queueing cancellation without joining its kernel completions allowed afterStopped to release/reuse storage while an old operation or cancellation still referenced the same user_data. The splice cancel/reuse test exposed an unexpected completion for inactive storage during broader validation.

## Decision

Track pending native submissions and pending cancellation acknowledgements in existing private request flags. Tag cancellation user_data using the low alignment bit so both completion kinds retain their owning request and are distinguished without allocation. A cancelling request filters normal success and error, clears each pending flag only when its corresponding CQE is dispatched, and delays afterStopped until both are drained. Do not submit a cancellation for an operation already drained by the collected batch. Requests without native submissions (timers, wake-ups and manually posted work) do not wait for nonexistent CQEs. Poll removal follows the same lifetime boundary. Discard descriptor results created by a successful accept/open that cancellation suppresses.

Active requests still report native errors, including existing EINTR retry behavior. A sender stop token whose owner command has not run does not yet put its native request into Cancelling; reported error identity still wins in that delivery window. Keep public layouts, allocation policy and adapter APIs unchanged.

## Confirmation

Reserve and release a report-mapped TCP port, collect connection refusal without dispatching, stop the request, dispatch the batch, then drive cancellation to afterStopped. Require one stop callback, zero normal callbacks and free reusable storage. Retain native splice cancel/reuse and timer/wake-up/manual cancellation coverage. Exercise epoll/io_uring and relevant Debug/Release platform suites; record exact failures and limits in the evidence log. This change does not claim to diagnose the separate intermittent normal lifetime-demo timeout on macOS/Windows.
