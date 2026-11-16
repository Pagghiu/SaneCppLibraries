# SOCKET-0006 - Avoid the OpenBSD blocking loopback connect wait

Status: Accepted
Date: 2026-11-16

## Context

On OpenBSD 8.0, a blocking loopback TCP connect can remain asleep in netcon after both endpoints reach
ESTABLISHED. SCTest encountered this in the synchronous setup for an AsyncFibers test.
[OpenBSD bug report](https://mail-archive.com/bugs%40openbsd.org/msg24662.html)

## Decision

For an OpenBSD loopback stream with blocking enabled and the default send timeout, submit connect with
O_NONBLOCK. Restore blocking mode before waiting for writable readiness, then read SO_ERROR to resolve
the result. This preserves blocking behavior for the caller and leaves the normal path in use for other socket
configurations.

## Confirmation

Run the existing Socket and AsyncFibers groups on the OpenBSD guest. Run SocketTest on other affected hosts
to check their normal connect path.
