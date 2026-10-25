# ASYNCUV-0001 - Experimental Distribution

Status: Accepted for the experiment
Date: 2026-10-25

## Context

AsyncUV is an experimental source-compatible libuv adapter with its own upstream test harness.
It needs the same distribution boundary as the other experiments.

## Decision

Keep the complete adapter, tests, and documentation under `Extra/Experimental/AsyncUV`.
Retain the `SCAsyncUVTest` target and explicitly select its sources in the build configuration.
Core libraries and `SC.cpp` do not depend on AsyncUV; ordinary single-file output excludes it.

## Consequences

AsyncUV remains outside the supported library set and carries no ABI or completeness guarantee.
Moving it does not expand its API or change the underlying Async implementation.

## Confirmation

The standalone target builds and runs from the new directory on macOS, Linux, and Windows.
Tracked source and build configuration contain no references to `SideProjects/AsyncUV`.

## Related

- [AsyncUV](../../Extra/Experimental/AsyncUV/README.md)
- [Experimental distribution policy](../../Extra/Experimental/README.md)
