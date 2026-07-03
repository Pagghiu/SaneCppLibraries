# COMMON-0007 - Keep IGrowableBuffer As the Minimal Output-Growth Adapter

Status: Accepted
Date: 2026-07-03

## Context

Several libraries need to append or read unbounded output without depending on Memory, Strings, Containers, or STL containers. A concrete output container would couple those libraries to an allocation policy. A large virtual interface would add unnecessary indirection and surface area.

## Decision

`IGrowableBuffer` remains the minimal type-erased output-growth adapter in Common. It exposes direct access to current storage and a single growth callback for the case where capacity is insufficient. Fixed-storage adapters can fail growth without allocation, while allocation-capable containers can opt in through their own specializations.

## Consequences

Libraries can write output into caller-owned, fixed, Sane-owned, STL, or custom containers without depending on the concrete container library. Callers keep allocation policy. The adapter contract must stay small and layout-conscious because it is shared by many libraries.

## Confirmation

A change preserves this decision when output-producing APIs can accept `IGrowableBuffer` or specific lightweight adapters instead of concrete owning containers, growth remains explicit and failure-aware, and the adapter does not grow into a general container abstraction.

## Related

- [IGrowableBuffer](../../Libraries/Common/IGrowableBuffer.h)
- [SC-0012 - Support bring-your-own containers](../Global/sc-0012-support-bring-your-own-containers.md)
- [SC-0011 - Make allocation-capable facilities explicit and optional](../Global/sc-0011-make-allocation-capable-facilities-explicit-and-optional.md)
