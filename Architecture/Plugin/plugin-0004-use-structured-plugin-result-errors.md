# PLUGIN-0004 - Use structured Plugin result errors

Status: Accepted
Date: 2026-09-14

## Context

Plugin previously represented failures with message strings across metadata scanning, filesystem access, compiler and
linker invocation, dynamic loading, and registry management. Several of these failures need an operation stage, a
native error number, a process exit status, or a required capacity without making that data part of the generic Result.

## Decision

Plugin owns category 7 and the portable `PluginError` taxonomy. `ResultPlugin` composes the authoritative `Result`
with a `uint16_t` operation detail, a `uint16_t` context kind, and one 32-bit tagged scalar context. Details distinguish
Plugin stages and OS-qualified native calls; they do not create platform-specific primary errors. Context factories
represent native errors, signed process exit codes, required byte capacity, and required element capacity.

Plain and foreign `Result` conversion deliberately clears Plugin detail and context. Same-domain copying preserves
them. Conversion to plain `Result` preserves only the structured category/error identity. The bridge layout is 24 bytes
on 64-bit supported targets and becomes 16 bytes after the legacy Result message pointer is removed.

The optional `PluginErrorFormatter.h` follows the Common caller-storage formatter contract. It accepts enum, plain, and
enriched values, rejects foreign categories and unknown primary/detail/context values, and formats signed exit codes
without allocation.

Expected completion remains successful: absent or malformed metadata, empty directories, iterator exhaustion, optional
symbols, absent interface hashes, no matching standalone lookup, and no-op load/unload states do not become errors.

## Consequences

Callers can branch on portable Plugin errors and inspect stable operation context without parsing prose. Diagnostic
formatting remains an opt-in header dependency. The structured bridge adds temporary ABI cost, and diagnostics beyond
the primary identity are intentionally lost at plain Result boundaries.

## Confirmation

Tests assert category and numeric stability, layout and POD properties, same-domain and foreign propagation, context
factories, exact formatter behavior including undersized and unknown inputs, scanner completion semantics, iterator
completion/error separation, Process failure identity preservation, and compiler/linker exit-code context. The category
registry validator, Plugin tests, and single-file compilation continue to pass.

## Related

- [COMMON-0009 - Use Library-Owned Structured Result Errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error category registry](../Common/result-error-categories.md)
- [Plugin architecture](plugin-architecture.md)
