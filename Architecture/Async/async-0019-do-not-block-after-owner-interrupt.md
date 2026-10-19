# ASYNC-0019 - Do Not Block After Owner Interrupt

Status: Accepted
Date: 2026-10-19

## Context

Submission can synchronously deliver an error callback before the kernel poll. That callback may interrupt the loop because its consumer has completed. beforeBlockingPoll listeners can also interrupt. Checking interruption only in the outer run loop still lets the current step block on unrelated handles or timers. A bounded filesystem sender consumer consequently waited for its watchdog and reported the watchdog failure instead of the already-delivered submission error.

## Decision

When interruption is already set at the kernel-poll boundary, use a nonblocking poll for that step. Continue its ordinary completion dispatch; interruption does not cancel requests or join their storage. Keep the existing nonblocking choices for manual completions and ready cancellation callbacks. Loop close still resets interruption while draining cancellation.

This changes neither native error identity nor handle ownership. It makes interruption during submission or the before-poll listener obey the same return-without-another-blocking-wait intent as interruption during completion dispatch.

## Confirmation

A portable direct Async regression queues a worker request against an uncreated pool, interrupts in its submission-error callback and leaves an unrelated long timer active. The timer must remain unexpired when run returns and the original ThreadPoolNotCreated result must remain intact. Repeat with interruption from beforeBlockingPoll. The old implementation deterministically reaches the sentinel timer before returning. Exercise both Linux backends and available macOS/Windows, Debug/Release; retain default-suite and integration checks. No assertion of the historical lifetime-demo timeout's cause follows from this independent reproduction.
