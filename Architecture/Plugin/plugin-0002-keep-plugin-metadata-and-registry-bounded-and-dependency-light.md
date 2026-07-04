# PLUGIN-0002 - Keep Plugin Metadata and Registry Bounded and Dependency-Light

Status: Accepted
Date: 2026-07-04

## Context

Plugin scanning, metadata parsing, dependency lists, build options, compiler paths, error logs, and registry entries all invite owning containers and higher-level filesystem/string helpers. Earlier dependency cleanup deliberately removed several convenient library dependencies to keep Plugin's integration footprint smaller.

## Decision

Plugin metadata and registry storage stay bounded and caller-visible. Public metadata uses fixed strings, fixed vectors, `StringPath`, spans, and caller-provided registry storage. File traversal and string/path helpers needed by Plugin live as Plugin-owned internal helpers instead of adding dependencies on higher-level Sane libraries.

## Consequences

Plugin has fixed limits for identifiers, dependencies, build options, file lists, and registry entries. Callers must provide enough storage and handle failures when metadata does not fit. In return, Plugin remains easier to ship as a standalone library and avoids hidden allocation through metadata handling.

## Confirmation

A change preserves this decision when Plugin public metadata remains bounded or caller-provided, registry capacity is explicit, scanning reports insufficient storage through `Result`, and new convenience helpers do not reintroduce dependencies on Memory, Containers, Strings, FileSystem, FileSystemIterator, Time, or STL containers.

## Related

- [Plugin public API](../../Libraries/Plugin/Plugin.h)
- [Plugin internal string helpers](../../Libraries/Plugin/Internal/PluginString.h)
- [Plugin tests](../../Tests/Libraries/Plugin/PluginTest.cpp)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [SC-0012 - Support bring-your-own containers](../Global/sc-0012-support-bring-your-own-containers.md)
