# SC-0004 - Single-File Libraries Are First-Class Distribution Artifacts

Status: Accepted
Date: 2026-07-02

## Context

Ease of integration is one of the core project goals. Many users should be able to drop one generated header into an existing project without adopting the repository layout, `SC::Build`, or the rest of the library set. Treating single-file libraries as a secondary packaging trick would let internal dependency mistakes and public-header assumptions accumulate unnoticed.

## Decision

Amalgamated single-file libraries are first-class distribution artifacts. Library structure, public headers, internal includes, common fragments, and dependency metadata must remain compatible with single-file generation and compilation.

## Consequences

Changes must consider both repository builds and generated single-file builds. Public headers and common fragments need stricter hygiene than ordinary internal code. Some implementation sharing is duplicated across generated outputs, which is acceptable when it preserves standalone consumption.

## Confirmation

A change preserves this decision when the single-file amalgamators can still generate libraries, `SCSingleFileLibs:` still compiles, generated outputs do not reference raw `Libraries/Common/...` includes, and consuming one generated library does not require the full repository tooling.

## Related

- [README usage options](../../README.md#how-to-use-sane-c-libraries-in-your-project)
- [Single File Amalgamation](../../Documentation/Pages/SingleFileLibs.md)
- [SC-0003 - Keep libraries independently consumable](sc-0003-keep-libraries-independently-consumable.md)
- [SC-0016 - Support layered adoption modes](sc-0016-support-layered-adoption-modes.md)
