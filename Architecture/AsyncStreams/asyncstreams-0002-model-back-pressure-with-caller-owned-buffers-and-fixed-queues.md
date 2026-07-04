# ASYNCSTREAMS-0002 - Model Back-Pressure With Caller-Owned Buffers And Fixed Queues

Status: Accepted
Date: 2026-07-04

## Context

Streams need to absorb bursts and coordinate producers with slower consumers. A conventional stream implementation might allocate internal queues or buffers, but `AsyncStreams` must keep memory ownership visible and bounded.

## Decision

`AsyncStreams` models back-pressure with caller-provided `AsyncBuffersPool` storage and fixed request queues. Readable streams pause when no buffer or request slot is available. Writable streams report when they can accept more data. Pipelines resume readable streams only after pending writes have drained enough to make progress.

## Consequences

Callers must size buffers and queues for the workflow. Pipeline behavior is more explicit than allocation-backed streams, but memory use is bounded, back-pressure is visible, and exhaustion is reported through `Result` or pause/resume state instead of hidden allocation.

## Confirmation

A change preserves this decision when stream buffers and read/write queues are caller-provided spans, pipelines enforce shared buffer pools, full queues cause pause or explicit failure rather than allocation, and tests cover pause/resume and pending-write behavior.

## Related

- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [AsyncStreams documentation: implementation](../../Documentation/Libraries/AsyncStreams.md)
- [AsyncBuffersPool and AsyncPipeline](../../Libraries/AsyncStreams/AsyncStreams.h)
