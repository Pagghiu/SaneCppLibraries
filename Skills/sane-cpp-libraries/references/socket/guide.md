# Sane Socket

## Quick Use

Choose `socket` when the task is about raw synchronous networking or DNS lookup.

- Use `SocketDescriptor` to create and own the OS socket handle.
- Use `SocketClient` for connect, read, and write flows.
- Use `SocketServer` for bind, listen, and accept flows.
- Use `SocketIPAddress` to build endpoints from text and port values.
- Use `SocketDNS` when a host name must be resolved before connecting.

## When Not To Use

- Use `async` when the user needs event-loop driven socket I/O.
- Use `http` or `http-client` when the task is HTTP-specific.
- Prefer a higher-level library when the user does not need direct socket control.

## References

- [socket primitives](references/socket-primitives.md)
- Public docs: `Documentation/Libraries/Socket.md`
- Tests: `Tests/Libraries/Socket/SocketTest.cpp`
