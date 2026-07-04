# TIME-0001 - Separate Realtime, Monotonic, Relative, and High-Resolution Time Semantics

Status: Accepted
Date: 2026-07-04

## Context

Time values are easy to misuse when wall-clock time, monotonic elapsed time, relative durations, and high-resolution counters share one type. Comparing or subtracting values from different clock sources can be meaningless even when the underlying representation is compatible.

## Decision

Time exposes separate public types for `Realtime`, `Monotonic`, `Absolute`, `Relative`, and `HighResolutionCounter`. Realtime represents wall-clock milliseconds since epoch. Monotonic represents a non-decreasing clock. Relative represents durations. High-resolution counters preserve platform counter precision and provide explicit exact or approximate conversions.

## Consequences

Callers can choose the clock semantics they need and documentation can warn about invalid cross-clock comparisons. The API has more small types than a single timestamp wrapper, but that cost keeps intent visible. Platform-specific counter representation remains hidden behind conversion methods.

## Confirmation

A change preserves this decision when new time APIs keep clock domains explicit, comparisons or subtractions across clock sources remain documented as invalid or are prevented by type design, high-resolution conversions stay explicit, and Time tests cover normalization and conversion behavior.

## Related

- [Time documentation](../../Documentation/Libraries/Time.md)
- [Time public API](../../Libraries/Time/Time.h)
- [Time implementation](../../Libraries/Time/Time.cpp)
- [Time tests](../../Tests/Libraries/Time/TimeTest.cpp)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
