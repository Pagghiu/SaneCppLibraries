# Time Architecture

## Purpose

Time is the dependency-free clock and time-value library. It must provide small typed wrappers for realtime, monotonic, relative, and high-resolution measurements without becoming a scheduler, timer manager, or event-loop dependency.

## Architectural Shape

The public interface lives under `SC::Time` and includes typed units (`Nanoseconds`, `Milliseconds`, `Seconds`), duration values (`Relative`), wall-clock values (`Realtime`/`Absolute`), monotonic values (`Monotonic`), and platform high-resolution counters (`HighResolutionCounter`). Platform clock reads and counter conversions are implementation details.

Clock domains must remain explicit. Cross-clock comparisons are not meaningful just because two values share an integer representation.

## Boundaries

Time owns clock reads, value wrappers, parsing absolute times into local/UTC calendar fields, arithmetic, literals, and explicit conversions. It does not own sleeping, scheduling, async timers, event-loop timeout queues, task coordination, calendar/timezone policy beyond local/UTC parsing, or date/time formatting libraries.

## Similarities With Other Libraries

Time is a leaf library like Socket and Threading: no library dependencies, public Sane primitives, and native OS details hidden in implementation files. Like Common primitives, its small public layouts must stay conservative because many libraries may use them.

## Differences From Other Libraries

Unlike Threading, Time does not block execution or coordinate threads. Unlike Async, Time does not manage readiness or timer queues. Unlike Process or Socket, Time does not wrap a persistent native handle; it wraps values and clock snapshots.

## Inspirations

The evidenced inspirations are native OS clocks: realtime clocks for wall-clock time, monotonic clocks for elapsed time, and platform high-resolution performance counters for precise intervals. The public type split is inspired by the different semantics those clocks provide.

## Anti-Inspirations

Inferred anti-inspirations include one generic timestamp type for all clock domains, scheduler APIs hidden inside a value library, and timezone/calendar frameworks that would pull Time beyond low-level clock/value handling.

## Architectural Choices

- Keep clock domains represented by distinct types.
- Keep Time dependency-free.
- Keep scheduling and sleeping outside Time.
- Keep conversions explicit, especially for high-resolution counters.
- Keep local/UTC parsing modest and platform-backed.

## Explicitly Excluded Targets

- Event-loop timer management.
- Thread sleeping or scheduling APIs.
- Coroutine or task coordination.
- Full timezone database or calendar library behavior.
- Requiring downstream libraries to depend on Time for simple primitive timeout storage.

## Sources

- [Time documentation](../../Documentation/Libraries/Time.md)
- [Time public API](../../Libraries/Time/Time.h)
- [Time implementation](../../Libraries/Time/Time.cpp)
- [Time tests](../../Tests/Libraries/Time/TimeTest.cpp)
- [TIME-0001 - Separate realtime, monotonic, relative, and high-resolution time semantics](time-0001-separate-realtime-monotonic-relative-and-high-resolution-time-semantics.md)
- [TIME-0002 - Keep Time as a leaf clock/value library, not a scheduler](time-0002-keep-time-as-a-leaf-clock-value-library-not-a-scheduler.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0015 - Prefer maturing existing libraries over expanding scope](../Global/sc-0015-prefer-maturing-existing-libraries-over-expanding-scope.md)

## Decision Log

- [TIME-0001 - Separate realtime, monotonic, relative, and high-resolution time semantics](time-0001-separate-realtime-monotonic-relative-and-high-resolution-time-semantics.md)
- [TIME-0002 - Keep Time as a leaf clock/value library, not a scheduler](time-0002-keep-time-as-a-leaf-clock-value-library-not-a-scheduler.md)
