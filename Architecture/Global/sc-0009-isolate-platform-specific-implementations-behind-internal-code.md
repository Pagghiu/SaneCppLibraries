# SC-0009 - Isolate Platform-Specific Implementations Behind Internal Code

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ presents platform abstraction libraries for macOS, Windows, Linux, and other targets. The public API should stay coherent even when backends use very different OS mechanisms such as kqueue, epoll, io_uring, IOCP, POSIX calls, Win32 calls, or Apple frameworks.

## Decision

Platform-specific code is isolated in `.cpp` files or library-specific internal files. Small platform branches may live inside implementation functions, but larger backend differences belong in private internal `.inl` or implementation files included by the owning library. Public headers expose the portable interface and stable storage shape, not OS details.

## Consequences

Backend implementations can be maintained and tested independently without leaking platform concepts into every caller. The separation may introduce some internal indirection or duplicated backend code, but it protects public API stability and keeps single-file outputs organized.

## Confirmation

A change preserves this decision when public headers remain platform-neutral, OS headers stay in private implementation paths, backend-specific behavior is tested through the public library interface, and platform conditionals do not spread into unrelated libraries.

## Related

- [Coding style: Platform specific or internal code](../../Documentation/Pages/CodingStyle.md#platform-specific-or-internal-code)
- [SC-0007 - Keep public headers free of system and compiler headers](sc-0007-keep-public-headers-free-of-system-and-compiler-headers.md)
