# SC-0008 - Prefer Native OS APIs Over Third-Party Dependencies

Status: Accepted
Date: 2026-07-02

## Context

The project aims to be easy to integrate and free of third-party build dependencies. macOS and Windows often provide stable native APIs for common platform abstractions, while Linux sometimes requires user-space libraries for functionality that is not part of the kernel. Uncontrolled third-party dependencies would weaken single-file adoption and build-system independence.

## Decision

When practical, libraries use native operating-system APIs instead of third-party dependencies. If a third-party dependency is unavoidable, it must be optional or hidden behind a Sane abstraction, must not impose a mandatory build system on library users, and must be documented as part of the consuming library's architecture.

## Consequences

Implementations may differ substantially by platform, and Linux may require special policy decisions for user-space facilities. The project may defer features when there is no dependency-safe cross-platform shape. This keeps the default integration story small and predictable.

## Confirmation

A change preserves this decision when new external dependencies are avoided by default, wrapped behind explicit APIs when necessary, absent from public headers, and do not become mandatory for unrelated libraries or simple single-file consumption.

## Related

- [Contributing: No external dependencies](../../CONTRIBUTING.md#no-external-dependencies)
- [Project principles](../../Documentation/Pages/Principles.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
