# API and dependency boundaries

Use this reference when publishing a library API or adding a dependency.

## Public API questions

- Is ownership visible: borrowed view, caller-owned output, returned owner, or explicit transfer?
- Is allocation policy visible to the caller who pays for it?
- Are optional inputs actually nullable/optional, and are required inputs non-optional in the host language's convention?
- Can an operation fail without leaving a half-valid object? If so, use a fallible create/init/assign boundary or document the invariant.
- Does the API expose the necessary capacity and cancellation choices, rather than burying them in a helper?

## Header hygiene and independence

Keep platform headers and platform-specific implementation behind internal or `.cpp` boundaries where practical. Avoid exposing a dependency solely because it is convenient internally. Prefer a small adapter, a caller-provided interface, or duplicated tiny glue when that preserves independent consumption and stable build costs.

Record a significant new dependency, allocation model, platform policy, or public lifetime trade-off as an architecture decision in projects that use ADRs. The decision should say why the alternative was not chosen and how the boundary will be checked.

## Legitimate exceptions

A public header may need a platform type, templates may be the right zero-overhead boundary, and a high-level product may deliberately choose managed/shared lifetime. Treat these as explicit design choices with documented build, ownership, and failure implications—not violations to hide.
