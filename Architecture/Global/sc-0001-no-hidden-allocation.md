# SC-0001 - Library Code Must Not Hide Dynamic Allocation

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ Libraries target platform abstraction and reusable low-level components where memory ownership must stay explicit. Hidden heap allocation makes latency, failure handling, dependency boundaries, and single-file integration harder to reason about.

## Decision

Library code must not perform hidden dynamic allocation. APIs that need bounded or unbounded storage accept caller-provided spans, typed storage, or explicit caller-owned allocation adapters. Allocation-capable libraries such as Memory and Containers expose allocation as their purpose or as caller-visible storage policy, not as an invisible side effect.

## Consequences

Most library APIs report storage exhaustion through `Result`-style failures and leave retry/allocation choices to the caller. Library-specific ADRs should reference this decision when choosing fixed storage, movement hooks, growable adapters, or intentional exceptions.

## Confirmation

A change preserves this decision when normal library paths do not call heap allocation APIs, storage is passed in or configured explicitly, out-of-storage cases return `Result` failures where possible, and any allocation-capable behavior is visible in the public type or option being used.

## Related

- [Project principles](../../Documentation/Pages/Principles.md)
- [README No Allocations](../../README.md#no-allocations-)
