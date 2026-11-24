# FOUNDATION-0005 - Match BSD Exact-Width Integer Types

Status: Accepted
Date: 2026-11-10

## Context

Foundation exposes exact-width integer aliases without pulling system headers into public library headers.
On the tested BSD ARM64 systems the system 64-bit types are long and unsigned long. Equal width alone
does not make long long interchangeable in C++ overloads or safe with system printf format specifiers.

## Decision

Use the compiler's exact-width type definitions on NetBSD, FreeBSD, and OpenBSD. Keep the other platform aliases
as they are. On 64-bit BSD, size types and exact-width integer types share the existing Strings formatter
specializations because the underlying types are identical.

## Confirmation

Build the existing Foundation LimitsTest against the BSD system typedefs and exercise the existing
integer formatting tests. Regenerate and compile the standalone libraries from the same alias definitions.
