# AWAIT-0003 - Keep Await Helpers Thin And Keep Long-Lived Streams Out Of AwaitEventLoop

Status: Accepted
Date: 2026-07-04

## Context

It is tempting to put every coroutine convenience method on `AwaitEventLoop`. Some workflows, such as filesystem watching or channel-like event streams, need long-lived state, buffering policy, overflow behavior, and stable objects beyond a single async request.

## Decision

Methods on `AwaitEventLoop` should remain thin wrappers over one underlying `Async` operation or small no-allocation loops over caller-provided storage. Helpers that carry protocol state, event queues, watcher state, or buffering policy should live in explicit caller-owned `Await*` helper objects instead of expanding `AwaitEventLoop`.

## Consequences

`AwaitEventLoop` stays a focused facade, and callback-style async workflows can coexist with coroutine code until a concrete bounded adapter is justified. Some coroutine conveniences require external helper objects rather than a single method call, but their storage and overflow behavior stay visible.

## Confirmation

A change preserves this decision when new `AwaitEventLoop` methods map closely to existing `AsyncRequest` shapes, long-lived streams or watchers are not added as thin loop methods, and helpers with extra state expose caller-owned storage and explicit failure/overflow behavior.

## Related

- [AWAIT-0001 - Wrap caller-owned AsyncEventLoop instead of owning a coroutine runtime](await-0001-wrap-caller-owned-asynceventloop-instead-of-owning-a-coroutine-runtime.md)
- [Await documentation: filesystem watcher integration](../../Documentation/Libraries/Await.md)
- [Await documentation: helper placement](../../Documentation/Libraries/Await.md)
