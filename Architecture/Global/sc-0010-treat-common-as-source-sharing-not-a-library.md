# SC-0010 - Treat Common as Source Sharing, Not a Library

Status: Accepted
Date: 2026-07-02

## Context

Many libraries need the same small primitives, but making every consumer depend on `Foundation` or another common library undermines the dependency cleanup and single-file story. At the same time, copy-pasting primitives manually would make fixes and agent navigation worse.

## Decision

`Libraries/Common` is a source-sharing folder, not a Sane C++ library. Common files are included into consuming libraries as fragments. They are not build targets, dependency targets, or generated project entries. Generated single-file libraries may inline common fragments into each consuming output, even when that duplicates code across outputs.

## Consequences

Common fragments must obey stricter rules than ordinary private implementation files because they can appear in multiple public or generated contexts. The same code may be duplicated across single-file libraries, but that duplication preserves independent consumption without introducing a new dependency node.

## Confirmation

A change preserves this decision when dependency metadata does not point to `Common`, generated projects do not create a `Common` target, single-file outputs do not reference raw `Libraries/Common/...` includes, and common fragments keep the normal Sane constraints.

## Related

- [Libraries/Common agent guidelines](../../Libraries/Common/AGENTS.md)
- [SC-0003 - Keep libraries independently consumable](sc-0003-keep-libraries-independently-consumable.md)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
