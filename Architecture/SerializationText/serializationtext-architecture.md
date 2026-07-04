# SerializationText Architecture

## Purpose

SerializationText serializes reflected C++ objects to and from structured text formats. Its current concrete format is JSON, but the library's architectural center is the reflection-based structured text walker, not a dynamic JSON document model.

## Architectural Shape

SerializationText has generic exact and versioned walkers that call a concrete stream implementation. JSON supplies `SerializationJson::Writer`, `SerializationJson::Reader`, and `JsonTokenizer`. Output flows through `SerializationTextOutput`, which appends to `IGrowableBuffer` and rolls back on formatting failure.

Exact reads follow the writer's field order. Versioned reads match object fields by reflected field name. JSON tokenization is cursor-based and returns slices into the original input; unescaping is performed only when the destination owns or can grow storage.

## Boundaries

SerializationText owns reflection traversal for structured text, JSON serialization/deserialization for reflected objects, JSON tokenization needed by that deserializer, and transactional text output. It does not own dynamic JSON values, JSON DOM editing, schema-free document construction, binary compatibility, or general string/container allocation policy.

The future dynamic JSON library must use the `JSON-NNNN` ADR scope and must not be retrofitted into SerializationText by accident.

## Similarities With Other Libraries

SerializationText shares Reflection's explicit schema model and SerializationBinary's exact/versioned split. It follows the Common `IGrowableBuffer` pattern to avoid depending on Memory, Strings, Containers, STL, or hidden allocation.

## Differences From Other Libraries

Unlike SerializationBinary, versioned text compatibility uses field names instead of member tags. Unlike Reflection, SerializationText interprets values and performs format-specific parsing. Unlike the reserved Json library, SerializationText does not create dynamic documents or expose a general JSON value tree.

## Inspirations

No external text serialization library is cited in local sources. The evidenced inspirations are the existing Reflection schema model, the Common growable-buffer adapter, and the project-wide goal of independently consumable no-allocation libraries.

## Anti-Inspirations

SerializationText is not a DOM library, not a pretty-printer-first formatting system, and not a string-owning parser. Inference: it intentionally avoids `nlohmann::json`-style dynamic value ownership and `std::string`-heavy APIs because those would conflict with caller-owned storage and independent library consumption.

## Architectural Choices

Keep JSON as a concrete stream under generic structured text walkers. Do not move reflection traversal into JSON-specific code.

Keep borrowed strings as borrowed slices. If JSON escapes require new bytes, the destination must provide writable/growable storage.

Keep `IGrowableBuffer` as the output abstraction. Writers may format into any compatible caller-provided buffer, but must not force a concrete string type or leave partial output after failure.

## Explicitly Excluded Targets

- Dynamic JSON document or DOM support in this scope.
- `Architecture/Json/` or `JSON-NNNN` decisions for current SerializationText code.
- Hidden allocation for parsing, string unescaping, or output.
- Concrete dependencies on Memory, Strings, Containers, or STL containers.
- Binary serialization behavior or member-tag compatibility.

## Sources

- [Serialization Text documentation](../../Documentation/Libraries/SerializationText.md)
- [SerializationJson public API](../../Libraries/SerializationText/SerializationJson.h)
- [SerializationJson implementation](../../Libraries/SerializationText/SerializationJson.cpp)
- [SerializationText exact walker](../../Libraries/SerializationText/Internal/SerializationTextReadWriteExact.h)
- [SerializationText versioned walker](../../Libraries/SerializationText/Internal/SerializationTextReadVersioned.h)
- [SerializationText output](../../Libraries/SerializationText/Internal/SerializationTextOutput.h)
- [JsonTokenizer](../../Libraries/SerializationText/Internal/JsonTokenizer.h)
- [SerializationJson tests](../../Tests/Libraries/SerializationText/SerializationJsonTest.cpp)
- [JsonTokenizer tests](../../Tests/Libraries/SerializationText/JsonTokenizerTest.cpp)
- [COMMON-0007 - Keep IGrowableBuffer As the Minimal Output-Growth Adapter](../Common/common-0007-keep-igrowablebuffer-as-the-minimal-output-growth-adapter.md)
- [SERIALIZATIONTEXT-0001 - Keep a Format-Neutral Reflection Walker Under JSON](serializationtext-0001-keep-a-format-neutral-reflection-walker-under-json.md)
- [SERIALIZATIONTEXT-0002 - Support Exact and Field-Name Versioned Text Reads](serializationtext-0002-support-exact-and-field-name-versioned-text-reads.md)
- [SERIALIZATIONTEXT-0003 - Use IGrowableBuffer Transactional Output Instead of Strings/Memory](serializationtext-0003-use-igrowablebuffer-transactional-output-instead-of-strings-memory.md)
- [SERIALIZATIONTEXT-0004 - Tokenize JSON Without Building a DOM](serializationtext-0004-tokenize-json-without-building-a-dom.md)
- [SERIALIZATIONTEXT-0005 - Treat Borrowed StringSpans As Raw JSON String Slices](serializationtext-0005-treat-borrowed-stringspans-as-raw-json-string-slices.md)

## Decision Log

- [SERIALIZATIONTEXT-0001 - Keep a format-neutral reflection walker under JSON](serializationtext-0001-keep-a-format-neutral-reflection-walker-under-json.md)
- [SERIALIZATIONTEXT-0002 - Support exact and field-name versioned text reads](serializationtext-0002-support-exact-and-field-name-versioned-text-reads.md)
- [SERIALIZATIONTEXT-0003 - Use IGrowableBuffer transactional output instead of Strings/Memory](serializationtext-0003-use-igrowablebuffer-transactional-output-instead-of-strings-memory.md)
- [SERIALIZATIONTEXT-0004 - Tokenize JSON without building a DOM](serializationtext-0004-tokenize-json-without-building-a-dom.md)
- [SERIALIZATIONTEXT-0005 - Treat borrowed StringSpans as raw JSON string slices](serializationtext-0005-treat-borrowed-stringspans-as-raw-json-string-slices.md)
