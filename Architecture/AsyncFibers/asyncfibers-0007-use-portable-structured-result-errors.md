# ASYNCFIBERS-0007 - Use Portable Structured Result Errors

Status: Accepted
Date: 2026-09-25

## Context

The fiber-to-Async bridge has a few failures of its own, but most operations forward a result from Async, Fibers,
File, Socket, or caller work. Its pending-operation state and public methods store plain `Result`; enlarging them for
error text or native diagnostics would affect the cancellation-sensitive request lifecycle.

## Decision

AsyncFibers owns category 14 and one append-only `AsyncFibersError` enum for bridge-owned failures: cancellation,
execution context, command queue validation/capacity, exact-read EOF, and socket-send no-progress. The primary codes
name portable conditions, never native APIs. Results returned by composed libraries or caller work retain their
original category and value. Plain `Result` remains non-owning and text-free after the migration bridge is removed.
The mandatory `AsyncFibers.h` exposes only numeric identity; optional `AsyncFibersErrorFormatter.h` formats English
messages into caller storage when requested.

No cancellation, scheduling, command publication, or callback behavior changes as part of this error port. Any future
enriched result needs a separate size and lifetime review rather than borrowed text in plain `Result`.

## Consequences

`SC_TRY` keeps propagating composed errors without translation. Callers can distinguish bridge cancellation and
precondition failures without comparing diagnostic strings. Formatting is optional and replaceable for localization.

## Confirmation

Tests check representative producer identities, cancellation and command-queue outcomes, exact-read EOF, formatter
status, and foreign-result preservation. The category validator, all single-file targets, and complete Debug and
Release suites on macOS, Linux, and Windows must pass.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [ASYNCFIBERS-0006 - Keep Cancellation Completion-Aware Across Async Stop Races](asyncfibers-0006-keep-cancellation-completion-aware-across-async-stop-races.md)
