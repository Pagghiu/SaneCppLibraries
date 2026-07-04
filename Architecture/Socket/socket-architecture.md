# Socket Architecture

## Purpose

Socket is the synchronous, dependency-free networking library. It must expose native socket descriptors, addresses, DNS lookup, TCP/UDP client and server flows, timeout reads, and explicit socket options without becoming an async, threading, file, time, or protocol library.

## Architectural Shape

The public interface is centered on `SocketDescriptor`, `SocketIPAddress`, `SocketServer`, `SocketClient`, `SocketDNS`, `SocketNetworking`, and typed `SocketFlags`. The descriptor owns native socket lifetime. Server and client wrappers borrow descriptors and provide role-specific operations. Options are named descriptor/server operations rather than hidden policy.

System socket headers and platform constants belong in internal implementation files.

## Boundaries

Socket owns synchronous socket creation, configuration, connect, bind, listen, accept, read, write, DNS resolution, and process-level networking init/shutdown where a platform requires it. It does not own event loops, task scheduling, TLS, HTTP, WebSocket, streams, or application protocols.

Timeouts should remain representable without depending on Time. Concurrency should remain caller-owned.

## Similarities With Other Libraries

Socket follows the same native-handle pattern as File, Process, SerialPort, and Threading: public Sane primitives, explicit handle lifetime, `Result` failures, and platform-specific implementation hidden behind the public interface.

Like Time and Threading, Socket is a leaf library with no library dependencies.

## Differences From Other Libraries

Unlike Process and SerialPort, Socket intentionally does not depend on File even though POSIX sockets are file descriptors. Unlike Async, Socket is synchronous. Unlike Http, Socket does not encode protocol rules above raw byte transport.

## Inspirations

The evidenced inspiration is native OS socket APIs: descriptors, address families, socket types, protocols, `setsockopt`-style options, DNS lookup, and Windows networking initialization. Socket should keep these concepts visible only where they are necessary for portable networking behavior.

## Anti-Inspirations

Inferred anti-inspirations include callback-driven networking frameworks, hidden background threads, protocol stacks, and convenience APIs that silently set transport options. Socket should not make TCP/UDP policy choices that callers cannot inspect or override.

## Architectural Choices

- Keep Socket synchronous and dependency-free.
- Keep descriptor lifetime explicit through `SocketDescriptor`.
- Keep TCP/UDP/address-family choices typed.
- Keep socket options visible as named operations.
- Keep platform networking init explicit.

## Explicitly Excluded Targets

- Async socket operations and event-loop integration.
- TLS, HTTP, WebSocket, or stream abstractions.
- File-descriptor inheritance from File as a library dependency.
- Thread ownership or background receive loops.
- Time-library dependency for timeout storage.

## Sources

- [Socket documentation](../../Documentation/Libraries/Socket.md)
- [Socket public API](../../Libraries/Socket/Socket.h)
- [Socket implementation](../../Libraries/Socket/Socket.cpp)
- [Socket tests](../../Tests/Libraries/Socket/SocketTest.cpp)
- [SOCKET-0001 - Keep Socket synchronous and dependency-free](socket-0001-keep-socket-synchronous-and-dependency-free.md)
- [SOCKET-0002 - Expose Socket options as explicit descriptor operations](socket-0002-expose-socket-options-as-explicit-descriptor-operations.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)

## Decision Log

- [SOCKET-0001 - Keep Socket synchronous and dependency-free](socket-0001-keep-socket-synchronous-and-dependency-free.md)
- [SOCKET-0002 - Expose Socket options as explicit descriptor operations](socket-0002-expose-socket-options-as-explicit-descriptor-operations.md)
