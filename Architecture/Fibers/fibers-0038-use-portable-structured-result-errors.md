# FIBERS-0038 - Use Portable Structured Result Errors

Status: Accepted
Date: 2026-09-22

## Context

Fibers has many literal error messages for stack management, worker pools, jobs, tasks, and cooperative synchronization.
Public task and job records also retain arbitrary `Result` values returned by caller procedures. Replacing those
results with a Fibers-only wrapper would either enlarge hot records or risk erasing foreign error identities.

## Decision

Fibers owns category 12 and one append-only `FibersError` enum. Its codes describe portable conditions and operation
stages, never operating-system calls or native error constants. Distinct memory reserve, commit, decommit, release,
and protection stages remain visible because the public boundary retains plain `Result` without extra stage context.
Thread policy, signal-handler, and signal-stack stages likewise use portable names. Future native diagnostics may be
added as explicitly platform-qualified detail only when a bounded enriched result is justified by an API.

Fibers-owned producer failures use the Fibers category. Caller procedure results preserve their original category and
value through task and job records, groups, and scheduler propagation. `Cancelled`, `SlotUnavailable`,
`QueueUnavailable`, and `NoProgress` remain distinct error results while the corresponding APIs still expose them as
failure; ordinary successful completion is never coded as an error. A later API redesign may represent expected
backpressure or control states without errors, but must not silently change current behavior during this migration.

The normal Fibers header exposes the numeric enum without English strings. Canonical English messages live only in the
opt-in `FibersErrorFormatter.h`; callers may format on demand or supply translations. No error result owns memory or
references a mutable or borrowed string.

## Consequences

Existing `SC_TRY` callers keep working. During the legacy `Result::message` bridge, Fibers-owned structured failures
have no message pointer, so clients requiring text must opt into the formatter. The numeric identity remains stable
through plain-Result storage, at the cost of not retaining a native error number or the exact object for generic codes.

## Confirmation

Tests check category and error values, formatting status and capacity, representative producer failures, and retention
of foreign procedure results. The category registry validator and single-file build must pass. Platform validation
must cover macOS, Linux, and Windows in Debug and Release.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
- [Fibers architecture](fibers-architecture.md)
