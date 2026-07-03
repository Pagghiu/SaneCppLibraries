# COMMON-0003 - Use Unguarded Inl Files as Per-Consumer Implementation Source

Status: Accepted
Date: 2026-07-03

## Context

Some shared implementation code is useful in multiple libraries but must produce symbols or private helper types owned by each consuming library. Giving these fragments include guards or compiling them as a shared library would either suppress necessary per-consumer copies or create a new dependency target.

## Decision

Common `.inl` files are unguarded implementation-source fragments. They are included only from private `.cpp` or internal implementation files. A consuming library owns the resulting symbols and should wrap private helpers in a library-specific namespace when the fragment is not intended to expose global `SC::` names.

## Consequences

Each library can reuse implementation code without depending on another Sane library or on a Common build target. The same fragment may intentionally be duplicated across generated single-file outputs. Consumers must supply the required include context and keep `.inl` fragments out of public headers.

## Confirmation

A change preserves this decision when Common `.inl` files remain unguarded, document non-obvious include requirements, are included only from private implementation paths, and do not leak private helper names into public APIs.

## Related

- [Libraries/Common agent guidelines](../../Libraries/Common/AGENTS.md)
- [COMMON-0005 - Let each consuming library own its assert provider](common-0005-let-each-consuming-library-own-its-assert-provider.md)
- [SC-0009 - Isolate platform-specific implementations behind internal code](../Global/sc-0009-isolate-platform-specific-implementations-behind-internal-code.md)
