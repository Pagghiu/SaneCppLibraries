# ContainersReflection Architecture

## Purpose

`ContainersReflection` is the opt-in bridge that teaches Reflection and Serialization how to handle Sane container and Memory storage types. It exists to keep core Reflection and Serialization independent from allocation-capable container libraries.

## Architectural Shape

The library is intentionally small and specialization-heavy. Its headers provide template specializations and extension-point glue for types such as `Vector`, `Array`, `VectorMap`, `Buffer`, and `String`. Users include the bridge only when they combine these storage types with Reflection or Serialization.

The bridge should stay declarative: describe container shape, expose size/data/resize hooks, and connect existing serialization machinery. It should not become a new runtime abstraction layer.

## Boundaries

`ContainersReflection` owns reflection and serialization specializations for container-like Sane storage types. It does not own the containers, memory storage, reflection core, serialization formats, schema compiler, or text/binary serializer engines.

Core Reflection and Serialization must not gain direct dependencies on `Containers` or `Memory` merely to support these types.

## Similarities With Other Libraries

Like other Sane libraries, the bridge keeps dependencies explicit and favors compile-time structure over runtime indirection. It follows the same independent-consumption rule by moving optional coupling to a separate library.

## Differences From Other Libraries

Unlike most libraries, `ContainersReflection` is not a standalone feature surface for application code. Its purpose is integration. It is valuable because it localizes coupling, not because it exposes much behavior of its own.

## Inspirations

The evidenced inspiration is the Sane dependency-cleanup pattern: optional combinations live in bridge libraries instead of forcing core libraries to depend on each other. `ContainersReflection` follows the same shape as the project-wide independent-library and bring-your-own-container decisions.

## Anti-Inspirations

Inference: this library should not become a plugin registry, type-erased runtime reflection adapter, or serialization framework. Those would increase surface area and make the bridge deeper than the specific coupling it exists to host.

Inference: adding every possible type specialization here by default is not a goal; specializations belong here when they prevent a worse dependency edge.

## Architectural Choices

Keep container and Memory storage specializations in the bridge.

Keep core Reflection, SerializationBinary, and SerializationText independent from `Containers` and `Memory`.

Use compile-time specializations and forward declarations where practical.

Keep bridge headers explicit: users should include them when they need the integration.

## Explicitly Excluded Targets

Do not move container specializations back into Reflection or Serialization core libraries.

Do not add ownership, allocation, or serialization-format policy here.

Do not create broad runtime adapter interfaces unless a future ADR accepts that coupling.

Do not make this bridge a mandatory include for normal container use.

## Sources

- [ContainersReflection documentation](../../Documentation/Libraries/ContainersReflection.md)
- [SC-0003 - Keep libraries independently consumable](../Global/sc-0003-keep-libraries-independently-consumable.md)
- [CONTAINERSREFLECTION-0001 - Keep Container Reflection and Serialization specializations in a bridge library](containersreflection-0001-keep-container-reflection-and-serialization-specializations-in-a-bridge-library.md)
- [ContainersReflection header](../../Libraries/ContainersReflection/ContainersReflection.h)
- [ContainersSerialization header](../../Libraries/ContainersReflection/ContainersSerialization.h)
- [MemoryReflection header](../../Libraries/ContainersReflection/MemoryReflection.h)
- [MemorySerialization header](../../Libraries/ContainersReflection/MemorySerialization.h)
- [Reflection tests](../../Tests/Libraries/Reflection)
- [SerializationBinary tests](../../Tests/Libraries/SerializationBinary)
- [SerializationText tests](../../Tests/Libraries/SerializationText)

## Decision Log

- [CONTAINERSREFLECTION-0001 - Keep Container Reflection and Serialization specializations in a bridge library](containersreflection-0001-keep-container-reflection-and-serialization-specializations-in-a-bridge-library.md)
