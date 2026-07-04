# SERIALIZATIONTEXT-0001 - Keep a Format-Neutral Reflection Walker Under JSON

Status: Accepted
Date: 2026-07-04

## Context

Serialization Text currently exposes JSON, but the reflection traversal rules are not JSON-specific. Putting traversal directly into the JSON reader and writer would make it harder to add another structured text format and would duplicate exact/versioned walking logic.

## Decision

Serialization Text keeps format-neutral exact and versioned reflection walkers under the JSON implementation. A concrete format implements stream callbacks such as `startObject`, `endObject`, `startArray`, `endArray`, `startObjectField`, and `serialize`; the generic walkers decide how to traverse reflected types and containers.

## Consequences

JSON owns tokenization and formatting, while shared serialization rules stay in reusable templates. Adding a new text format still requires a stream implementation, but not a full rewrite of reflection traversal. Some JSON-specific concerns must not leak back into the generic walkers.

## Confirmation

A change preserves this decision when `SerializationTextReadWriteExact` and `SerializationTextReadVersioned` remain format-neutral, JSON behavior is expressed through `SerializationJson::Reader` and `SerializationJson::Writer`, and new text formats can reuse the walkers without depending on JSON tokens.

## Related

- [Serialization Text documentation: architecture](../../Documentation/Libraries/SerializationText.md)
- [Serialization Text exact walker](../../Libraries/SerializationText/Internal/SerializationTextReadWriteExact.h)
- [Serialization Text versioned walker](../../Libraries/SerializationText/Internal/SerializationTextReadVersioned.h)
- [SerializationJson stream implementation](../../Libraries/SerializationText/SerializationJson.h)
