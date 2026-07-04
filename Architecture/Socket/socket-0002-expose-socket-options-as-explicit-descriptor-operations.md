# SOCKET-0002 - Expose Socket Options as Explicit Descriptor Operations

Status: Accepted
Date: 2026-07-04

## Context

Socket behavior depends on platform socket flags and options such as blocking mode, descriptor inheritability, address reuse, TCP_NODELAY, broadcast, multicast membership, multicast loopback, hop limits, and outbound interfaces. Hiding those policies in server/client helpers would make behavior surprising and difficult to port.

## Decision

Socket options are explicit descriptor or server operations. `SocketDescriptor::create` accepts address family, socket type, protocol, blocking, and inheritability choices. Additional options are set through named methods such as `setTcpNoDelay`, `setBroadcast`, multicast controls, and `SocketServer::bind` reuse-address options.

## Consequences

Callers must opt into transport-level behavior instead of relying on hidden defaults. The public API exposes some native networking concepts, but platform-specific constants and system headers remain private. Tests can verify each behavior through the public API.

## Confirmation

A change preserves this decision when new socket flags or options are exposed through explicit typed API choices, hidden policy in connect/listen/read/write helpers is avoided, platform-specific constants stay in implementation files, and Socket tests cover externally visible option behavior.

## Related

- [Socket public API](../../Libraries/Socket/Socket.h)
- [Socket POSIX implementation](../../Libraries/Socket/Internal/SocketDescriptorPosix.inl)
- [Socket Windows implementation](../../Libraries/Socket/Internal/SocketDescriptorWindows.inl)
- [Socket tests](../../Tests/Libraries/Socket/SocketTest.cpp)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
