# SC-0005 - Avoid STL, Exceptions, and RTTI in Library Code

Status: Accepted
Date: 2026-07-02

## Context

The project exists partly as a deliberate C++ subset that favors low compile times, small binaries, predictable debugging, and explicit ownership. Standard-library containers, exceptions, and RTTI can introduce hidden allocation, runtime dependencies, compile-time cost, and control flow that does not fit the rest of the project.

## Decision

Library code avoids the C++ Standard Library, exceptions, and RTTI. Public Sane APIs use `SC::` primitives and explicit result propagation instead. External users may still adapt `std::` or third-party containers at the edges when an API intentionally supports caller-owned storage or growable adapters.

## Consequences

The project must provide small primitives such as `Result`, `Span`, `StringView`, `StringSpan`, `Function`, handle wrappers, and placement helpers instead of assuming standard-library facilities. Some code is more explicit than idiomatic modern C++, but it remains portable across no-stdlib and low-dependency builds.

## Confirmation

A change preserves this decision when library code does not introduce `std::` containers, exceptions, RTTI, standard-library runtime requirements, or forbidden standard headers, and when interoperability with external containers remains explicit at API boundaries.

## Related

- [Project principles](../../Documentation/Pages/Principles.md)
- [Coding style: Exceptions / RTTI](../../Documentation/Pages/CodingStyle.md#exceptions--rtti)
- [SC-0006 - Use explicit Result-based error propagation](sc-0006-use-explicit-result-based-error-propagation.md)
- [SC-0012 - Support bring-your-own containers](sc-0012-support-bring-your-own-containers.md)
