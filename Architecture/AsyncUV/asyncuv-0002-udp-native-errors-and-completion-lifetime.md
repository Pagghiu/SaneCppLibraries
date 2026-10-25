# ASYNCUV-0002 - UDP Native Errors and Completion Lifetime

Status: Accepted for the experiment
Date: 2026-10-25

## Context

Libuv UDP callbacks expose native socket errors. Async reports portable operation failures, and reading
`errno` or the Winsock error later in a completion callback can observe an unrelated error. Hosted macOS
multicast tests exposed this difference.

## Decision

Reuse the adapter's nonblocking native UDP send helper when no datagrams await Async backpressure handling.
Store the immediate result in the caller-owned request, then notify its callback through a zero-timeout request.
Retain Async for sends that would block and preserve their native send order.

Track accepted send callbacks independently from pending native sends. UDP close waits for those callbacks
before delivering its close callback. Already-completed native sends do not serialize their completion timers
on the native send sequence.

## Consequences

Immediate errors retain their original identity and callbacks remain asynchronous without allocations.
This does not change Async's portable error contract or add native error storage to Async.

## Confirmation

The Linux errno-preservation regression must report `UV_EACCES` after `errno` is overwritten.
Upstream `udp_send_immediate` must observe both send callbacks and both close callbacks.
The hosted macOS multicast-interface tests must report their actual native send result.
