# HTTP-0005 - Allow externally owned async transports

Status: Accepted
Date: 2026-08-20

## Context

Some supported OS transports own DNS, connection establishment, TLS, and listener acceptance. Network.framework is
one example and has no public API for adopting a connected or accepted BSD socket. Http currently creates every client
socket and server listener itself, so its post-connect transport hooks begin too late for these providers.

Http must remain independent from Tls, Https, and platform APIs. Existing socket and byte-pump TLS paths must continue
to work without adopting a second HTTP state machine.

## Decision

Allow `HttpAsyncClient` to delegate connection establishment to an optional external connector after URL parsing and
before DNS or socket creation. The connector receives the parsed endpoint, caller-owned HTTP connection storage, and
completion/failure callbacks. It installs ready plaintext `AsyncReadableStream` and `AsyncWritableStream` objects on
the connection before completing.

Allow `HttpAsyncServer` to run in an external-listener mode and accept plaintext stream pairs injected on its event-loop
thread. Http activates a normal caller-provided connection slot and runs the existing HTTP/1.1 state machine on those
streams.

Connection retirement and slot reuse follow the installed transport streams, not specifically the built-in socket
streams. External providers retain ownership of native handles and use transport close/shutdown hooks to drive their
asynchronous lifetime barriers. Http never names TLS, certificates, ALPN, or a platform provider.

## Consequences

Native transport owners can compose with Http without private socket-adoption APIs or an Http-to-Tls dependency. The
connector and listener adapters must provide stable caller-owned storage until their stream close barrier completes.
Http gains a small generic ownership surface and keeps the current socket path as its default.

## Confirmation

Tests must prove that an external client connector runs before DNS, may complete asynchronously, supports origin reuse
and reconnect, and cannot cause premature stream reuse. Server tests must prove externally injected streams use the
existing parser/response path, respect pool exhaustion, and finish stream destruction before slot reuse. Dependency
generation and standalone amalgamation must continue to show no Http dependency on Tls or Https.

## Related

- [HTTP-0001 - Keep HTTP on caller-provided async connection storage](http-0001-keep-http-on-caller-provided-async-connection-storage.md)
- [HTTP-0004 - Keep transport concerns in HttpAsyncClient](http-0004-keep-transport-concerns-in-httpasyncclient-outside-message-types.md)
