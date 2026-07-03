# COMMON-0005 - Let Each Consuming Library Own Its Assert Provider

Status: Accepted
Date: 2026-07-03

## Context

Assertions and backtrace printing are useful across many libraries, but a shared Foundation-owned assert implementation would force libraries to depend on Foundation for diagnostics. At the same time, duplicating all assert code manually in each library would be noisy and error-prone.

## Decision

`Assert.h` provides shared assertion declarations and macros, while `Assert.inl` provides implementation source. Each consuming library declares its own provider with `SC_DECLARE_ASSERT_PROVIDER`, defines library-specific assert and trust-result macros near its export macro, and includes `Assert.inl` once from that library's implementation with the provider selected.

## Consequences

Each library owns its assert symbols and can remain independent from Foundation. The assert implementation can still be shared and kept consistent. Consumers must set up the provider correctly, and `Assert.inl` may include system headers because it is implementation-only source.

## Confirmation

A change preserves this decision when libraries that need assert implementation define their own provider, no library gains a Foundation dependency solely for assertions, `Assert.inl` is included only from implementation files, and public headers use only the guarded assert declarations/macros.

## Related

- [Assert declaration](../../Libraries/Common/Assert.h)
- [Assert implementation fragment](../../Libraries/Common/Assert.inl)
- [COMMON-0003 - Use unguarded inl files as per-consumer implementation source](common-0003-use-unguarded-inl-files-as-per-consumer-implementation-source.md)
