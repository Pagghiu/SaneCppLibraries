@page library_time Time

@brief 🟨 Wall-clock timestamps, monotonic time, durations, and high-resolution measurement

[TOC]

[SaneCppTime.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppTime.h) is a small,
allocation-free layer over the platform clocks. It provides enough vocabulary to timestamp an event, measure elapsed
time, express a duration without passing a bare integer, and break an epoch timestamp into local or UTC calendar
fields. It is not a calendar, timezone, formatting, or timer-scheduling library.

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Time.svg)


# Choosing The Right Clock

The important choice is not resolution but meaning:

| Need | Type | Representation and tradeoff |
|:-----|:-----|:----------------------------|
| A timestamp that can be stored or compared with an external system | `Time::Realtime` | Signed milliseconds since the Unix epoch. The operating system can adjust this clock, so elapsed-time measurements can jump. |
| A deadline or elapsed interval that must ignore wall-clock corrections | `Time::Monotonic` | Signed milliseconds from the platform monotonic clock. Its origin is intentionally unspecified, so the value is meaningful only relative to another reading from the same clock. |
| Finer-grained elapsed-time measurement | `Time::HighResolutionCounter` | The native performance counter on Windows and `CLOCK_MONOTONIC` seconds/nanoseconds on POSIX. It preserves the platform counter until conversion. |
| A duration | `Time::Nanoseconds`, `Time::Milliseconds`, `Time::Seconds`, or `Time::Relative` | Integer unit wrappers are exact in their unit. `Relative` stores fractional seconds as a `double`, which is convenient for conversion but is not an exact duration representation. |

Do not compare or subtract `Realtime` and `Monotonic` values merely because both derive from `Time::Absolute`: their
epochs and clock behavior differ. Similarly, a `HighResolutionCounter` is for measurements inside the current process,
not a serializable timestamp.

# Measuring And Comparing Time

For ordinary timeout bookkeeping, take two `Time::Monotonic::now()` readings and use `subtractExact()` to obtain
integer milliseconds. Use `HighResolutionCounter` when retaining the platform counter's finer resolution matters. A
counter is default-constructed as a zero/reference value; call `snap()` before treating it as the current instant.

This compiled test demonstrates the comparison and offset model without waiting on the wall clock:

@snippet Tests/Libraries/Time/TimeTest.cpp highResolutionCounterIsLaterOnSnippet

`subtractExact()` on high-resolution counters returns another counter containing the precise native difference.
Convert that difference with `toNanoseconds()`, `toMilliseconds()`, or `toSeconds()`. `subtractApproximate()` instead
returns `Time::Relative`, converting the difference to floating-point seconds. Integer conversions discard sub-unit
precision, except `Relative` conversions, which round by adding half a unit before truncation.

The `_ns`, `_ms`, and `_sec` literals make the unit visible at call sites, as the preceding test's `123_ms` does. They
construct SC duration wrappers and do not sleep or schedule anything. `offsetBy()` is useful for constructing a
comparison point, but it does not arrange a wake-up. Use
[Async](@ref library_async) for event-loop timers, [Await](@ref library_await) for coroutine sleeps and deadlines, or
the relevant blocking primitive when a thread should actually sleep.

# Wall-Clock Timestamps And Calendar Fields

`Time::Realtime::now()` returns the platform's current wall-clock time in milliseconds since the Unix epoch. An
existing epoch value can also be wrapped directly in `Realtime` or `Absolute`. `parseUTC()` and `parseLocal()` write
calendar fields into a caller-provided `Time::Absolute::ParseResult`; `parseLocal()` uses the process environment's
local timezone and daylight-saving rules.

This test parses and formats a current local timestamp:

@snippet Tests/Libraries/Time/TimeTest.cpp absoluteParseLocalSnippet

Parsing itself allocates no memory. `ParseResult` owns fixed-size buffers for the localized month and weekday names,
and `getMonth()` and `getDay()` return pointers into those buffers. The result must therefore outlive those pointers.
The numeric fields follow the platform `tm` conventions: `month` is zero-based, `dayOfWeek` uses Sunday as zero, and
`dayOfYear` is zero-based. The displayed names come from the active C locale. Both parse functions return `false` if
the platform conversion or name generation fails.

Formatting is intentionally outside this library. The example uses [Strings](@ref library_strings) to build text;
applications can instead keep the numeric fields, use a caller-provided string buffer, or hand them to another
formatting layer.

# Allocation, Lifetime, And Portability

All Time values are small value types. The library performs no dynamic allocation and retains no caller memory. There
are no handles to close and no clock object whose lifetime must be managed. The only output object is the
caller-provided `ParseResult`.

The API deliberately exposes the common denominator of Windows, macOS, and Linux clocks. Consequently it does not
promise a particular hardware resolution, monotonic-clock epoch, or timezone database. `Realtime` is millisecond
precision; `Monotonic` also returns milliseconds even when the underlying clock is finer. High-resolution conversion
is subject to integer range and truncation, and readings from different machines or processes should not be treated as
sharing an origin.

`Time::Absolute::offsetBy()` saturates positive overflow at `INT64_MAX`, but the duration and subtraction APIs are not
general checked-arithmetic facilities. Code accepting untrusted or extreme durations should validate them before
performing arithmetic.

# Fit And Current Limits

Time fits code that wants a dependency-free, allocation-free clock abstraction and explicit duration units while
staying close to operating-system semantics. It is also the time vocabulary used by neighboring SC libraries for
timeouts and timestamps: [Async](@ref library_async) schedules timers, [FileSystem](@ref library_file_system) reads and
writes file timestamps, and [Threading](@ref library_threading) provides blocking sleeps and synchronization.

Choose a fuller date/time library if the application needs timezone identifiers and transitions, parsing formatted
input, calendar arithmetic, ISO-8601 serialization, leap-second policy, or locale-controlled formatting. Those
features are intentionally absent today. The current 🟨 MVP status reflects this narrow surface and the lack of a
committed roadmap, not an allocation or dependency requirement left to the caller.

# API Reference

The principal types are `Time::Realtime`, `Time::Monotonic`, `Time::HighResolutionCounter`, `Time::Relative`, and the
three integer duration wrappers. See the [Time API group](@ref group_time) for their complete member reference after
choosing the clock model above.

# Further Reading

- [January 2025 Update](https://pagghiu.github.io/site/blog/2025-01-31-SaneCppLibrariesUpdate.html)

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Time`.
Single File counts
`SaneCppTime.h`.
Standalone counts `SaneCppTimeStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 133		| 208		| 341	|
| Single File | 238		| 303		| 541	|
| Standalone  | 238		| 303		| 541	|
