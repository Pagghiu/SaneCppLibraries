# ASYNCSTREAMS-0003 - Use A Fixed-Layout Pipeline Instead Of Dynamic Stream Graphs

Status: Accepted
Date: 2026-07-04

## Context

Node-style stream systems can support dynamic graphs, object mode, and unbounded listener or transform composition. Those features are convenient, but they usually require allocation, broader lifetime rules, or more complex ownership.

## Decision

`AsyncPipeline` uses a fixed layout: one source, a bounded array of transforms, and a bounded array of sinks. It does not provide object mode or a dynamically allocated stream graph. Event listener counts, transform counts, sink counts, and pending writes stay fixed inside the pipeline type.

## Consequences

Some flexible graph topologies are out of scope for the core pipeline. In exchange, pipeline memory use is predictable, teardown can unregister known listener sets, and the implementation remains compatible with standalone no-allocation use.

## Confirmation

A change preserves this decision when `AsyncPipeline` continues to expose fixed transform and sink capacities, does not allocate graph nodes or listener lists, and documentation continues to describe fixed layout and no object mode as intentional constraints.

## Related

- [AsyncStreams documentation](../../Documentation/Libraries/AsyncStreams.md)
- [AsyncPipeline](../../Libraries/AsyncStreams/AsyncStreams.h)
- [SC-0015 - Prefer maturing existing libraries over expanding scope](../Global/sc-0015-prefer-maturing-existing-libraries-over-expanding-scope.md)
