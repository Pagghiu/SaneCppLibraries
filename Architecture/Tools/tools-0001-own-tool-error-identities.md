# TOOLS-0001 - Keep Tool Errors Separate From Library Errors

Status: Accepted
Date: 2026-09-27

## Context

The build, package, and format tools use `Result` and historically embedded diagnostic literals. They are not
libraries, but their failures need stable identities before the temporary `Result.message` bridge can be removed.
Putting tool failures in Common or in an unrelated library would make library error taxonomies misleading.

## Decision

Tools own built-in Result categories as subsystems, registered in the same append-only category registry as libraries.
The common tool runner and formatter own `ToolsResultCategory` and `ToolsError` under `Tools/`. SC-build owns a separate
`BuildResultCategory` and `BuildError`; SC-package may likewise add its own category. Tool error enums are
mandatory only for the owning tool; canonical English formatters remain optional, write into caller-owned storage,
and are invoked by the command-line entry point that presents diagnostics. Foreign library Results propagate with
their original category and code.

## Consequences

Library headers do not acquire tool-specific errors. Tool executables that print canonical diagnostics intentionally
link presentation strings; consumers that only inspect a tool Result need not include its formatter. Category
registration and error values remain append-only.

## Confirmation

The category checker scans library and tool headers. Focused tool tests verify numeric identity, canonical formatting,
and the command-line presentation path. The final bridge audit must find no tool-owned literal-error factory or
`SC_TRY_MSG` call before removing `Result.message`.

## Related

- [COMMON-0009 - Use library-owned structured Result errors](../Common/common-0009-use-library-owned-structured-result-errors.md)
- [Result error formatting contract](../Common/result-error-formatting.md)
