# ASYNC-0017 - Preserve Collected Completions on Interrupt

Status: Accepted
Date: 2026-10-18

## Context

A loop interrupt can arrive after kernel polling but before dispatch. Discarding that collected batch loses consumed IOCP/io_uring completions permanently. Joined cancellation then correctly waits for a completion that was discarded. The split-phase API makes the failure deterministic; an early sender terminal notification exposed it while closing the owning runtime.

## Decision

Interruption ends run after its current collected batch is dispatched; it does not discard native completions or release active requests. A pre-existing interruption makes run return without starting another kernel poll, and run consumes the interruption even when there is no counted work. Explicit dispatchCompletions always processes its supplied batch. Callbacks in an already-collected batch may therefore run after an interrupt request, matching the existing behavior when interruption occurs inside another callback in that batch.

Closing ignores application interruptions while draining owned requests and cancellation completions, then clears the interruption. Share one private pending-work predicate between run and close so cancellation queues and pending native acknowledgements cannot be omitted from cleanup. No public API, allocation or layout changes.

## Confirmation

Collect a socket receive, interrupt before dispatch, and require the callback, payload and free request state. Consume the interrupt with run, then start another receive, interrupt and close; require cancellation cleanup and free request storage. Cover epoll/io_uring and available macOS/Windows Debug/Release suites. Retain the pre-fix failure and the parent-debugger stack showing the owning close waiting on the lost native wake-poll completion.
