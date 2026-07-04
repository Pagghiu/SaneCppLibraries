# ASYNCSTREAMS-0001 - Keep AsyncStreams Dependency-Free Through Templated Async Adapters

Status: Accepted
Date: 2026-07-04

## Context

`AsyncStreams` composes naturally with `Async`, but making it depend directly on `Async` would enlarge its standalone output and weaken independent library adoption. The core stream model only needs buffers, events, queues, and caller-provided hooks.

## Decision

`AsyncStreams` remains dependency-free at the library level. Core readable, writable, duplex, transform, buffer, event, and pipeline types do not depend on `Async`. File and socket stream adapters are templates over an event-loop/request type, so they depend on concrete async request types only when instantiated by a caller.

## Consequences

The adapter interfaces are more template-heavy than a direct `Async` dependency would be. In exchange, `AsyncStreams` can be used independently, and other async providers can adapt to the stream model if they provide compatible request shapes.

## Confirmation

A change preserves this decision when `Documentation/Libraries/AsyncStreams.md` still reports no library dependencies, `AsyncStreams.h` does not include `Async.h`, generated single-file outputs do not pull in Async by default, and request-backed streams remain templated adapters.

## Related

- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](../Global/sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
- [AsyncStreams documentation](../../Documentation/Libraries/AsyncStreams.md)
- [AsyncRequestStreams](../../Libraries/AsyncStreams/AsyncRequestStreams.h)
