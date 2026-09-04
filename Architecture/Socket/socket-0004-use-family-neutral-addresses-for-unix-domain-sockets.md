# SOCKET-0004 - Use Family-Neutral Addresses for Unix-Domain Sockets

Status: Accepted
Date: 2026-08-28

## Context

Socket operations previously accepted only `SocketIPAddress`, even though native stream and datagram operations also
work with Unix-domain addresses. Adding separate Unix-only connect, bind, accept, send-to, and receive-from operations
would duplicate the transport surface and force the same distinction into Async and Await. Unix pathname addresses also
need an exact native length, while Linux abstract names are byte sequences rather than filesystem paths.

The `File` library already exposes a portable named-pipe abstraction. That abstraction remains useful for portable local
IPC, but it does not replace callers that need native socket semantics such as datagrams or peer-address reporting.

## Decision

`SocketAddress` is the family-neutral address value for transport operations. It stores IPv4, IPv6, Unix pathname, and
Linux abstract addresses inline together with their exact native length. Existing `SocketIPAddress` overloads remain as
compatibility adapters.

Socket, Async, and Await use the same generic address for connect and unconnected datagram operations. Bind and accept
also expose it in Socket. Unix-domain stream and datagram sockets use `ProtocolDefault`; macOS and Linux support pathname
addresses, Linux additionally supports abstract names, and Windows and Emscripten report Unix-domain creation as
unsupported.

The socket descriptor owns only the native handle. It never unlinks a pathname before bind or after close; filesystem
endpoint lifetime remains explicit and caller-owned.

## Consequences

One transport surface now works across IP and local sockets without allocation or public system headers. Async and Await
inherit Unix-domain support without parallel request types. Callers that only use IP addresses keep their existing API.

The inline native storage has a fixed public size, platform support is intentionally asymmetric, and pathname callers
must remove stale and finished socket nodes themselves. Ancillary data, credentials, `SOCK_SEQPACKET`, and automatic
endpoint cleanup remain outside this decision.

## Confirmation

Tests cover Unix pathname parsing, stream connect/accept, datagram send/receive with source reporting, Linux abstract
names, both Linux Async backends, and Await forwarding. Windows builds and unsupported-platform tests confirm that the
public headers remain portable and Unix-domain creation fails explicitly.

## Related

- [Socket architecture](socket-architecture.md)
- [Socket documentation](../../Documentation/Libraries/Socket.md)
- [FILE-0001 - Represent Named Pipes as File Pipe Descriptors](../File/file-0001-represent-named-pipes-as-file-pipe-descriptors.md)
- [SOCKET-0003 - Expose unconnected datagrams as descriptor operations](socket-0003-expose-unconnected-datagrams-as-descriptor-operations.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
