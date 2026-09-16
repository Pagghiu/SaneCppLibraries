# SOCKET-0005 - Use structured Socket result errors

Status: Accepted
Date: 2026-09-20

## Context

Socket previously represented validation, address parsing, descriptor operations, DNS, stream I/O, datagram I/O, and
client/server failures with string literals or anonymous failed `Result` values. Callers could not reliably distinguish
retryable nonblocking I/O, timeouts, capacity failures, or native failures without parsing prose.

## Decision

Socket owns category 8 and one portable `SocketError` taxonomy. Primary errors describe platform-neutral conditions and
operations. `SocketErrorDetail` identifies the logical stage and, only where required to interpret native diagnostics,
the POSIX or Windows backend call. Platform names are forbidden in primary errors.

`ResultSocket` composes the authoritative `Result` with a `uint16_t` detail, a `uint16_t` context kind, and one tagged
32-bit scalar. Context may contain a native error, signed resolver error, required byte capacity, or actual transferred
byte count. The result owns no memory and retains no borrowed strings. It is 24 bytes during the legacy-message bridge
and 16 bytes after `Result` returns to its final eight-byte representation.

Native error context is captured immediately from `errno`, `WSAGetLastError()`, or `GetLastError()` only when the failed
API defines that diagnostic channel. `getaddrinfo()` results use resolver context rather than pretending to be native
socket errors. Validation failures and APIs without a defined numeric diagnostic leave context inactive.

Nonblocking `WouldBlock` and timed `TimedOut` remain non-success results to preserve existing public signatures and
caller control flow, but now have portable identities. A zero-length datagram and stream end-of-file remain successful
operations with an empty output span. A truncated datagram remains an error because the datagram is consumed without a
complete result.

Plain and foreign `Result` conversion clears Socket detail and context. Same-domain copies preserve them. Conversion to
plain `Result` preserves only category/error identity. The optional `SocketErrorFormatter.h` formats into caller-owned
storage and is not included by the mandatory Socket header.

## Consequences

Callers can distinguish retry, timeout, validation, capacity, DNS, and native failures without strings or allocation.
Existing callers using `SC_TRY` or storing a plain `Result` keep working and retain portable identity, although they
deliberately lose Socket-specific detail. English diagnostics remain link-optional and replaceable for translation.

The migration does not turn previously ignored platform calls into checked failures. Pre-existing behavioral bugs such
as POSIX shutdown-direction selection and Windows interruption handling require separate regression tests and commits.

## Confirmation

Contract tests assert category and numeric stability, bridge/final size, trivial copying, standard layout, context
factories, same-domain and foreign propagation, formatter sizing and exact output, and rejection of unknown values.
Producer regressions cover invalid addresses and sockets, address-in-use, would-block, timeout, datagram truncation,
native failures, resolver failures, and capacity reporting on supported platforms.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
- [Socket architecture](socket-architecture.md)
