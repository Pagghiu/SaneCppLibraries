@page library_socket Socket

@brief 🟨 Synchronous TCP, connected UDP, multicast configuration and DNS lookup

[SaneCppSocket.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSocket.h) provides a
small, synchronous portability layer over native sockets on Windows, macOS and Linux.

[TOC]

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Socket.svg)


# When to use Socket

Use Socket when an application wants direct control of a native network handle and blocking operations are acceptable:

- a small TCP client or server running on a dedicated thread;
- connected UDP request/response traffic;
- joining an IPv4 or IPv6 multicast group and configuring its interface, loopback and hop limit; or
- resolving a host name synchronously into an IPv4 or IPv6 text address.

Socket is intentionally below a protocol or stream framework. It does not provide TLS, HTTP, buffering, message framing,
automatic reconnects, readiness polling or an event loop. It also does not expose unconnected `sendto` / `recvfrom`
operations. Use [Async](@ref library_async) when sockets must share an event loop, [Await](@ref library_await) for
coroutine composition, [Async Streams](@ref library_async_streams) for queued and framed byte streams, and
[HTTP](@ref library_http) when the protocol is HTTP.

# The handle is the socket; client and server are views

SC::SocketDescriptor is the central abstraction. It exclusively owns one native `SOCKET` on Windows or file descriptor
on POSIX. It is move-only through SC::UniqueHandle: destruction or SC::SocketDescriptor::close releases the handle, and a
move transfers that responsibility. Creation chooses IPv4 or IPv6, stream or datagram, TCP or UDP, blocking mode and
child-process inheritance. The defaults are a blocking, non-inheritable TCP stream socket.

SC::SocketServer and SC::SocketClient do not own another resource. They are short-lived views over a descriptor:
SC::SocketServer adds bind, listen and accept operations, while SC::SocketClient adds connect, send and receive operations.
The descriptor must therefore outlive either view. An accepted TCP connection is returned as a new owning
SC::SocketDescriptor; the listening descriptor remains independently owned by the server.

The server side of the lifecycle is visible in this snippet compiled with `SocketTest`:

\snippet Tests/Libraries/Socket/SocketTest.cpp socketServerSnippet

SC::SocketServer::bind enables address reuse by default. This is convenient for restarting a server, but callers can
disable it or inspect SC::SocketServer::BindStatus when they need to distinguish an address already in use. `listen` and
`accept` are TCP operations. For UDP, bind the datagram descriptor and use a SC::SocketClient view to read from it; the
current synchronous UDP API models a connected peer rather than arbitrary per-datagram source addresses.

# Addresses, DNS and process initialization

SC::SocketIPAddress stores the native IPv4 or IPv6 address and port inline in fixed-size aligned storage. Parsing accepts
numeric ASCII addresses only; it does not resolve a host name. Converting back to text writes into a caller-provided
buffer of at least SC::SocketIPAddress::MAX_ASCII_STRING_LENGTH bytes and returns a view into that buffer.

Use SC::SocketDNS separately when a name must be resolved:

\snippet Tests/Libraries/Socket/SocketTest.cpp resolveDNSSnippet

Resolution blocks the calling thread, accepts ASCII host names, and returns one textual address selected from the native
resolver results. The caller supplies the result buffer; host names longer than the implementation's 255-byte temporary
buffer fail. This API does not expose the full resolver list, canonical names, service lookup or asynchronous
cancellation.

Call SC::SocketNetworking::initNetworking once before socket work and
SC::SocketNetworking::shutdownNetworking during orderly process shutdown. These calls manage Winsock on Windows and are
no-ops on POSIX. They are explicit because the library does not hide process-wide initialization in a descriptor
constructor.

# Reading and writing are single synchronous operations

SC::SocketClient::read performs one blocking native receive into an SC::Span supplied by the caller. Its output span is a
view of the bytes actually received: it may be shorter than the input buffer, and an empty span is the peer's orderly TCP
shutdown. `readWithTimeout` first waits for readability and returns an unsuccessful SC::Result when the timeout expires;
it does not allocate, cancel another operation or distinguish timeout with a dedicated status type.

SC::SocketClient::write likewise performs one native send. It succeeds only if the entire input span is sent; a partial
native send is reported as an error instead of being retried. Applications that require `writeAll`, protocol framing,
backpressure or queued buffers must implement that policy or use [Async Streams](@ref library_async_streams).

This accepted-client snippet shows the caller-owned read buffer and descriptor lifetime:

\snippet Tests/Libraries/Socket/SocketTest.cpp socketClientAcceptSnippet

All operations report failures through SC::Result. The layer deliberately keeps native socket semantics visible: a
blocking connect, accept, read, DNS lookup or full kernel send buffer can stall its thread. Changing a descriptor to
non-blocking mode does not turn these synchronous wrappers into an event-driven API; associate such a descriptor with
[Async](@ref library_async) instead.

# UDP, broadcast and multicast

Datagram descriptors use the same ownership model. The synchronous client/server facade supports UDP after connecting a
sender to one peer and binding a receiver. SC::SocketDescriptor additionally exposes broadcast enablement and multicast
group membership, loopback, hop-limit and outbound-interface controls for IPv4 and IPv6.

These are socket options, not a multicast protocol layer. The caller must provide family-compatible group and interface
addresses, choose ports, define packet boundaries at the application level and decide how loss, duplication and ordering
are handled. There is no source-address result on receive, and a datagram still has to fit the caller's buffer.

# Boundaries and tradeoffs

Socket keeps allocation and ownership predictable: addresses are inline values, descriptors own native handles, and I/O
uses caller-provided spans. Library operations do not allocate application buffers. Native resolver functions used by
SC::SocketDNS may allocate internally, so DNS lookup should not be treated as an allocation-free OS operation.

That small surface is a good fit for bounded synchronous networking and for creating handles that higher-level SC
libraries will drive. It is a poor fit when the application needs many concurrent connections, cancellation, portable
readiness notification, scatter/gather I/O, arbitrary UDP peers, TLS, or protocol-aware buffering. Those omissions are
part of the current interface, not policies silently supplied by the library.

For the complete option and method reference, see the [Socket module](@ref group_socket).

# Status

🟨 MVP

The tested surface covers synchronous IPv4/IPv6 TCP, connected UDP, read timeouts, DNS, broadcast and multicast options.
The API remains deliberately narrow and does not yet cover several facilities expected from a general-purpose socket
library, especially unconnected datagrams and richer I/O/status reporting.

# Blog

Some relevant blog posts are:

- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)
- [March 2026 Update](https://pagghiu.github.io/site/blog/2026-03-31-SaneCppLibrariesUpdate.html)

# Roadmap

🟩 Usable:
- Add unconnected UDP send/receive operations with peer-address reporting

🟦 Complete Features:
- Define richer timeout and partial-I/O status where callers need to distinguish outcomes

💡 Unplanned Features:
- TLS and application protocols belong in higher-level libraries

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Socket`.
Single File counts
`SaneCppSocket.h`.
Standalone counts `SaneCppSocketStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 185		| 894		| 1079	|
| Single File | 1158		| 1080		| 2238	|
| Standalone  | 1158		| 1080		| 2238	|
