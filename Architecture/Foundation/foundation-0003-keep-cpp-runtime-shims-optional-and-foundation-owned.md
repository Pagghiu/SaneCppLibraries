# FOUNDATION-0003 - Keep C++ Runtime Shims Optional and Foundation-Owned

Status: Accepted
Date: 2026-07-03

## Context

Some strict no-standard-library builds need tiny C++ runtime ABI shims, such as operators new/delete, guard functions, and compiler runtime hooks. These symbols are global and surprising, so providing them implicitly from arbitrary libraries would be dangerous. At the same time, no-stdlib Sane targets need one controlled place to opt into them.

## Decision

C++ runtime shims are optional and Foundation-owned. They are compiled only when `SC_PROVIDE_CPP_RUNTIME_SHIMS` is enabled for a target that intentionally avoids linking the C++ standard-library runtime. Other libraries must not provide their own duplicate runtime shims.

## Consequences

Strict no-stdlib builds have a single controlled implementation path for the minimal runtime symbols they need. Normal builds can link the platform C++ runtime and leave the shims disabled. Build tooling must reject contradictory configurations where both the standard runtime and Sane runtime shims would provide the same ABI symbols.

## Confirmation

A change preserves this decision when runtime shim symbols are emitted only through Foundation's opt-in path, targets do not link both standard runtime support and Sane shims, and libraries outside Foundation do not define duplicate global C++ runtime ABI symbols.

## Related

- [Foundation runtime shims](../../Libraries/Foundation/Internal/LibC++.inl)
- [SC::Build documentation](../../Documentation/Pages/Build.md)
- [Building user documentation](../../Documentation/Pages/BuildingUser.md)
- [SC-0016 - Support layered adoption modes](../Global/sc-0016-support-layered-adoption-modes.md)
