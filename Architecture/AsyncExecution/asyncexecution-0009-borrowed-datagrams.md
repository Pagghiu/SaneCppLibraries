# ASYNCEXECUTION-0009 - Borrowed Datagram Senders

Status: Accepted
Date: 2026-10-18

## Context

Async, Await and AsyncFibers expose sendTo/receiveFrom. Sender composition needs equivalent one-datagram operations without stream-style retry loops, while preserving peer identity and caller-owned buffer lifetimes. Oversized receives must be distinguishable from complete messages.

## Decision

Add context.sendTo(socket, address, data) for scalar or vector payloads, accepting SocketIPAddress or SocketAddress. Copy the destination address into the sender; borrow the socket, bytes and vector metadata through terminal completion. An empty vector uses a scalar empty payload to send one empty datagram consistently. Submit exactly one native send-to request, never chunk or retry a partial datagram as another message. Successful values report the native transferred byte count. The protocol's maximum message size and unsupported address families remain native errors. Reject counts beyond the common uint32 native buffer domain before narrowing, including vector-sum overflow.

Add context.receiveFrom(socket, buffer), emitting AsyncExecutionSocketReceiveFromResult with a borrowed data span, copied sourceAddress/sourceSocketAddress and truncated. The compatibility IP address is invalid for a non-IP peer. A receive consumes one datagram, including zero-byte messages and zero-capacity buffers. Empty messages do not imply EOF. ASYNC-0015 defines the shared truncation behavior; never claim the original datagram length from a prefix.

Use the existing lazy operation, cancellation and terminal delivery machinery. Stop completes only after native quiescence; receiving storage may be destroyed from the terminal callback. Preserve Result identity through the error channel and outputs on error/stopped. Do not add an owning datagram socket type or a second event loop. Non-template implementations remain in AsyncExecution.cpp.

## Confirmation

Exercise provider when_all with scalar/vector sends, copied destination addresses, empty messages, truncation and the next message, zero-capacity receive, IPv4/IPv6, prestop/active cancellation, terminal destruction and socket reuse. Check overflow and native invalid-socket errors. Run focused and full Debug/Release/platform and amalgamation gates before checking AE-043.
