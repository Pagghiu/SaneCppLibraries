# SOCKET-0003 - Expose Unconnected Datagrams as Descriptor Operations

Status: Accepted
Date: 2026-08-24

## Context

Socket supports connected UDP through the client/server views, while synchronous callers also need to exchange individual
datagrams with arbitrary peers and identify each sender. Native `recvfrom` truncation differs across platforms: POSIX can
report success with a truncated prefix, while Winsock reports `WSAEMSGSIZE`.

## Decision

`SocketDescriptor` exposes allocation-free `sendTo` and `receiveFrom` operations using caller-provided spans and
`SocketIPAddress`. A successful receive publishes both the received-data span and source address. Every failure leaves
those outputs unchanged.

An oversized datagram is consumed and reported as failure on every platform. Its truncated prefix may already have been
written into the caller's buffer. Would-block is also an unsuccessful `Result`; richer structured I/O status remains a
separate future decision.

## Consequences

Synchronous code can use connected or unconnected UDP without depending on Async or Await. Datagram boundaries and peer
addresses remain explicit and allocation-free. Callers must size receive buffers for their protocol and cannot recover a
datagram after truncation. Callers needing portable readiness or structured completion status must compose with a
higher-level library.

## Confirmation

A change preserves this decision when public APIs use caller-owned storage, complete datagrams publish source addresses,
would-block does not mutate outputs, oversized datagrams fail consistently on POSIX and Windows, and IPv4/IPv6 tests cover
round trips, zero-length datagrams and truncation.

## Related

- [Socket architecture](socket-architecture.md)
- [Socket documentation](../../Documentation/Libraries/Socket.md)
- [Socket public API](../../Libraries/Socket/Socket.h)
- [SOCKET-0001 - Keep Socket synchronous and dependency-free](socket-0001-keep-socket-synchronous-and-dependency-free.md)
