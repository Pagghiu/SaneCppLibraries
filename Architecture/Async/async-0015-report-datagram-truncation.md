# ASYNC-0015 - Report Datagram Truncation

Status: Accepted
Date: 2026-10-18

## Context

A receive-from operation consumes one datagram, including when the supplied buffer is too small or the datagram is empty. The existing POSIX recvfrom path silently returned a prefix, while Winsock reported WSAEMSGSIZE as a receive failure. Sender consumers need to distinguish an empty message from stream EOF and an incomplete message from a complete one without inventing a larger original length.

## Decision

Add truncated to AsyncSocketReceive::CompletionData and expose Result::isTruncated(). It defaults false for stream receives. Datagram completion reports the copied prefix, its source address and whether bytes were not returned. The result does not claim the original message length.

Use recvmsg and MSG_TRUNC on POSIX readiness backends, and inspect the persistent recvmsg header on io_uring. On Winsock, normalize UDP WSAEMSGSIZE into a successful truncated prefix. Handle immediate WSAEMSGSIZE with manual completion because the receive was not initiated and has no later completion indication; pending receives inspect their normal overlapped completion. Preserve other receive errors. Native zero-byte datagrams remain successful messages and never set disconnected.

Keep request and buffer ownership unchanged. Bounds, cancellation and exactly-one terminal delivery still apply. Each new receive resets the Windows immediate-truncation marker; a later normal datagram must not inherit truncation state. This uses stack or existing request storage and adds no allocation or library dependency.

## Consequences

Callers can reject truncated messages explicitly while retaining the observed prefix and source. UDP excess bytes are discarded by the native socket; they must not become a second receive. A zero-capacity receive can consume a nonempty datagram and report an empty truncated prefix. This is distinct from consuming an empty complete datagram. Existing POSIX successful-prefix behavior gains information; Windows UDP oversized receives now provide the same successful-prefix contract.

## Confirmation

Bounded callback tests cover both already-queued and pending receives: oversized message into a short buffer, a following small message, an empty message and a nonempty message into a zero-capacity buffer. Check content, copied count, source port, truncation, no EOF, one callback and independent request reuse. Run forced epoll and io_uring on Linux and available macOS/Windows Debug/Release gates. Sender coverage also checks IPv4/IPv6 and cancellation.

## References

- [POSIX recvmsg](https://pubs.opengroup.org/onlinepubs/9799919799/functions/recvmsg.html)
- [Microsoft WSARecvFrom message sizes and overlapped completion](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsarecvfrom)
