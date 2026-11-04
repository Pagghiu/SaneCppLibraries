# HTTP-0009 - Use low-latency TCP for the built-in server listener

Status: Accepted
Date: 2026-11-02

## Context

The HTTP server can send a response header and its body as separate asynchronous writes. With Nagle's algorithm and
delayed acknowledgments, a small response can incur a platform-dependent delay on every keep-alive request. On the
tested NetBSD and FreeBSD guests, this prevented the existing 64-request stress case from meeting its deadline.

## Decision

Enable TCP_NODELAY on the TCP listener created by `HttpAsyncServer::start`. Accepted sockets inherit this setting on
the tested macOS, Linux, Windows, NetBSD and FreeBSD systems. Propagate an error if the setting cannot be applied.
Applications can set a different value on an accepted socket through the transport setup hook. External listeners and
injected streams retain their owner's socket policy.

## Consequences

Small responses can be delivered promptly without application code or tests choosing platform-specific options.
Disabling Nagle may increase packet counts for workloads that issue small writes. The option is applied once when the
listener starts; it does not add a system call for each accepted connection.

## Confirmation

Verify the default and caller override on accepted sockets using the portable Socket query, run the existing HTTP
stress and integration tests across supported hosts, and compare the HTTP file-server benchmark before and after.
