# CONTAINERSREFLECTION-0001 - Keep Container Reflection and Serialization Specializations in a Bridge Library

Status: Accepted
Date: 2026-07-04

## Context

`Reflection`, `SerializationBinary`, and `SerializationText` need to understand container-like types such as `Vector`, `Array`, `Buffer`, and `String` when users opt into those combinations. Making the core reflection or serialization libraries depend on `Containers` or `Memory` would add allocation-capable dependencies to libraries that should stay independently consumable.

## Decision

Container, buffer, and string reflection or serialization specializations live in the `ContainersReflection` bridge library. Users include this bridge when they want Reflection or Serialization to handle Sane container and Memory storage types. The core Reflection and Serialization libraries do not own those specializations.

## Consequences

The dependency graph stays smaller for users who do not serialize Sane containers. The bridge library carries the coupling between container storage and reflection metadata explicitly. Adding support for more allocation-capable storage types belongs in the bridge unless the core library already depends on that storage type for another accepted reason.

## Confirmation

A change preserves this decision when Reflection and Serialization dependency metadata do not gain `Containers` or `Memory` dependencies for these specializations, `ContainersReflection` remains the place where the relevant template specializations are included, and examples/tests include the bridge when container serialization support is required.

## Related

- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [ContainersReflection documentation](../../Documentation/Libraries/ContainersReflection.md)
- [ContainersReflection](../../Libraries/ContainersReflection/ContainersReflection.h)
- [ContainersSerialization](../../Libraries/ContainersReflection/ContainersSerialization.h)
- [MemoryReflection](../../Libraries/ContainersReflection/MemoryReflection.h)
