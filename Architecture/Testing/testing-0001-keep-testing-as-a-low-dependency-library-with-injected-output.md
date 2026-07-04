# TESTING-0001 - Keep Testing As a Low-Dependency Library with Injected Output

Status: Accepted
Date: 2026-07-04

## Context

Testing is used by every library and by the repository test executable, so dependencies added to Testing can quickly spread through the project. The test framework still needs to print useful output, handle paths, and report `Result` failures without depending on higher-level Strings or Memory libraries.

## Decision

Testing remains a low-dependency library built on Common primitives. `TestReport` receives output through an injected `IOutput` interface, stores paths in fixed `StringPath` buffers, and records expectations using `StringSpan` and `Result` without owning higher-level string containers.

## Consequences

Tests can use richer libraries, but the Testing library itself stays independently consumable and single-file friendly. Console formatting is supplied by the embedding executable through the output adapter. Testing's own public API must stay careful about not pulling in optional libraries just for convenience.

## Confirmation

A change preserves this decision when dependency reports keep Testing free of Memory, Strings, Containers, and STL dependencies, output still flows through `TestReport::IOutput`, path state uses fixed Common path storage, and tests can still run through `SCTest` with a concrete console adapter.

## Related

- [Testing documentation](../../Documentation/Libraries/Testing.md)
- [Testing public API](../../Libraries/Testing/Testing.h)
- [SCTest main](../../Tests/SCTest/SCTest.cpp)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
