# SOCKET-0001 - Keep Socket Synchronous and Dependency-Free

Status: Accepted
Date: 2026-07-04

## Context

Socket is the low-level networking library used by synchronous callers and higher-level async libraries. Pulling in File, Threading, Time, Async, or string/container libraries would make a basic socket descriptor much heavier and would weaken single-library adoption.

## Decision

Socket remains a synchronous, dependency-free networking library. It exposes descriptors, client/server helpers, DNS lookup, address parsing, read/write, timeout reads, and networking initialization through Common primitives and primitive timeout values. Threading, async scheduling, and higher-level protocol behavior stay outside Socket.

## Consequences

Socket can be consumed standalone and used as a small native OS abstraction. Callers that need concurrency, async operation, or richer time abstractions compose Socket with other libraries themselves. The synchronous API may be less convenient for event-loop users, but it keeps the dependency graph clean.

## Confirmation

A change preserves this decision when Socket documentation still reports no library dependencies, public Socket APIs do not require File, Threading, Time, Async, Strings, Memory, Containers, or STL types, and timeout/concurrency behavior remains representable without depending on those libraries.

## Related

- [Socket documentation](../../Documentation/Libraries/Socket.md)
- [Socket public API](../../Libraries/Socket/Socket.h)
- [Socket tests](../../Tests/Libraries/Socket/SocketTest.cpp)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](../Global/sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
