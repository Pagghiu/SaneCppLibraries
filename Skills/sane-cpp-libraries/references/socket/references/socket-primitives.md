# Socket Primitives

## Use This When

- Open or accept TCP or UDP sockets directly.
- Resolve host names to socket endpoints.
- Build a simple client or server without HTTP semantics.

## Main Types

- `SC::SocketDescriptor`
- `SC::SocketServer`
- `SC::SocketClient`
- `SC::SocketIPAddress`
- `SC::SocketDNS`
- `SC::SocketNetworking`

## Common Flows

### Client

1. Create a `SocketDescriptor`.
2. Use `SocketDNS` or `SocketIPAddress` to resolve or construct the endpoint.
3. Use `SocketClient::connect`.
4. Read and write with the client helpers.

### Server

1. Create a `SocketDescriptor`.
2. Wrap it in `SocketServer`.
3. Bind and listen.
4. Accept a new `SocketDescriptor` for each connection.

## Pitfalls

- Create the descriptor before calling the server or client helpers.
- Distinguish blocking socket work from async request-driven work.
- Treat DNS lookup as part of endpoint setup, not as a networking transport layer.

## Best Sources

- `Documentation/Libraries/Socket.md`
- `Tests/Libraries/Socket/SocketTest.cpp`
- `Libraries/Socket/Socket.h`
