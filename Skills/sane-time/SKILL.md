---
name: sane-time
description: Sane C++ time handling for relative, absolute, monotonic, realtime, and high-resolution counters. Use when measuring durations, parsing local time, scheduling work, or choosing the correct time source.
---

# Sane Time

## Overview

Use this skill when the user needs to measure time, represent timestamps, or choose between monotonic and realtime semantics. Keep the guidance tied to the public `SC::Time` types.

## Use This Skill When

- A request asks which time source to use for elapsed measurements.
- A request asks how to represent wall-clock versus interval time.
- A request asks how to parse or compare time values.
- A request asks how to time a task without depending on the standard library.

## Start Here

- Read [references/time-types-and-usage.md](references/time-types-and-usage.md).
- Inspect `Libraries/Time/Time.h` and `Time.cpp`.
- Use `Tests/Libraries/Time/TimeTest.cpp`.

## Key Guidance

- Prefer monotonic time for elapsed-duration measurement.
- Prefer realtime time for wall-clock timestamps.
- Use absolute time when the user needs a parsed or fixed local instant.
- Use `HighResolutionCounter` for fine-grained profiling-style questions.

## Pitfalls

- Do not use wall-clock time for elapsed intervals.
- Do not assume the same precision or semantics as `std::chrono`.
- Do not overcomplicate the answer when the user only needs a source selection.
