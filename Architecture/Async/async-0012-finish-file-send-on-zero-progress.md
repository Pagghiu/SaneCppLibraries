# ASYNC-0012 - Finish File Send on Zero Progress

Status: Accepted
Date: 2026-10-18

## Context

Borrowed file-send parity requires a bounded terminal result when a requested range extends past EOF. POSIX sendfile returns zero at EOF; both native reactivation and worker loops previously treated this as recoverable socket pressure. A worker could spin indefinitely. The Windows worker instead returned success with an incomplete range. Intermediate failures also discarded observed byte counts on several backends.

## Decision

Treat a zero-byte file-send result before the requested range is complete as FileSendIncomplete. Preserve the completed byte prefix in CompletionData on success and error. Native Windows errors retain their existing identity; a successful native completion with a short count also becomes FileSendIncomplete. Never infer transferred bytes from requested size or a preflight file-size snapshot.

Record whether the backend actually selected a zero-copy transfer mechanism for an observed prefix. This reports the mechanism, not a physical memory-copy guarantee. Worker fallback preserves its non-zero-copy status and rejects successful zero-progress socket writes rather than looping.

Observe the two Linux io_uring splice stages separately. Submit pipe-to-socket work only for bytes actually supplied by file-to-pipe completion, drain that prefix before reading another chunk, and fail on zero progress. Keep one private pending-byte counter in Linux requests; discard the internal pipe before a new independent start so cancelled leftovers cannot leak into another range. This changes Linux request layout in the experimental branch.

Keep native request ownership, cancellation quiescence and explicit worker selection unchanged. EOF does not trigger an automatic copied-I/O fallback or silently grow the requested range. The correction belongs to Async and is shared by its adapters.

## Consequences

Windows worker users now receive an error for an incomplete range instead of a successful result with isComplete false. POSIX EOF finishes without requiring file growth, event-loop polling or a worker join that can never return. Operational failures remain observable alongside their known completed prefixes; unknown kernel-side transfer counts are not guessed.

## Confirmation

Use a requested range longer than the file in native and forced-worker modes. An independent guard grows the file only if completion stalls, releasing the old POSIX loop so the baseline fails boundedly. Corrected runs must finish without growth, report an error and expose the observed prefix where the backend reports it. Retain normal content-transfer tests, explicit offsets and platform-specific error identity checks.
