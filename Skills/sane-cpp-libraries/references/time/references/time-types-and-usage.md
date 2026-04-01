# Time Types And Usage

## What To Teach First

- `SC::Time::Relative` for durations and intervals.
- `SC::Time::Monotonic` for elapsed-time measurement.
- `SC::Time::Realtime` for wall-clock timestamps.
- `SC::Time::Absolute` for parsed or fixed local instants.
- `SC::Time::HighResolutionCounter` for fine-grained measurement.

## Best Files To Inspect

- `Libraries/Time/Time.h`
- `Libraries/Time/Time.cpp`

## Best Examples

- `Tests/Libraries/Time/TimeTest.cpp`

## Common Advice

- Use monotonic time for profiling or elapsed timing.
- Use realtime time for user-facing timestamps.
- Keep absolute parsing separate from interval measurement.
- Mention platform semantics only when the caller needs them.

## Handoff

- Route scheduling or timeout logic that depends on async behavior to `async`.
