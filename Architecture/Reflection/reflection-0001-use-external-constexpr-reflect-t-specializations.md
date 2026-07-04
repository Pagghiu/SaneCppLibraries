# REFLECTION-0001 - Use External constexpr Reflect&lt;T&gt; Specializations

Status: Accepted
Date: 2026-07-04

## Context

Reflection exists primarily to provide schema information for serialization without hidden allocation, RTTI, exceptions, STL, or intrusive base classes. Automatic reflection techniques either depend on newer C++ features, compiler-specific tricks, or heavier metaprogramming than the project wants in a public single-file library.

## Decision

Reflection descriptions are provided externally by specializing `SC::Reflection::Reflect<T>` and implementing a `constexpr` visitor, usually through the `SC_REFLECT_STRUCT_*` macros. The reflected type does not need to inherit from a framework type or contain reflection metadata.

## Consequences

User code must repeat field tags, names, member pointers, and offsets, but the schema remains explicit, portable across supported compilers, and usable in `constexpr` contexts. Automatic reflection experiments stay outside the core Reflection library unless a new decision accepts them.

## Confirmation

A change preserves this decision when reflected structs are described through `Reflect<T>` specializations or the local macros, schema compilation remains possible at compile time, and new reflection mechanisms do not require RTTI, hidden allocation, intrusive base classes, or C++20-only field-name extraction.

## Related

- [Reflection documentation](../../Documentation/Libraries/Reflection.md)
- [Reflection public API](../../Libraries/Reflection/Reflection.h)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
- [SC-0005 - Avoid STL, exceptions, and RTTI in library code](../Global/sc-0005-avoid-stl-exceptions-and-rtti-in-library-code.md)
