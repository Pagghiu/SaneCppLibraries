@page library_serialization_text Serialization Text

@brief 🟨 Map reflected C++ data to and from JSON in caller-controlled storage

[TOC]

[SaneCppSerializationText.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppSerializationText.h) is the single-file distribution. Serialization Text currently means one concrete format: `SC::SerializationJson`, built on compile-time [Reflection](@ref library_reflection).

# Dependencies
- Dependencies: [Reflection](@ref library_reflection)
- All dependencies: [Reflection](@ref library_reflection)

![Dependency Graph](SerializationText.svg)


# When It Fits

Use Serialization Text when an application already has reflected, value-like C++ structures and needs compact JSON
without introducing a DOM, exceptions, or a mandatory allocator. It walks the object graph directly: reflected field
names become JSON object keys, fixed and vector-like containers become arrays, and supported scalar and string values
become JSON values.

This is a small mapper rather than a general JSON toolkit. It is a good fit for application state, configuration, and
interchange data whose schema is controlled by the program. It is currently a weaker fit when you need arbitrary JSON
trees, preservation of unknown fields, incremental I/O, rich format controls, or a mature compatibility layer.

The library is marked MVP. JSON is the only implemented format, the public operation reports only `bool`, and the
supported type and conversion surface is intentionally narrower than a full JSON implementation.

# The Model: Reflection Drives The Walk

Serialization Text does not inspect C++ declarations by itself. A type opts in through [Reflection](@ref
library_reflection), including a stable text name for every field. This compiled test model mixes primitive values, a
fixed C array, an owning string, and a vector of strings:

@snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonModelSnippet

`SC::String` and `SC::Vector` are not dependencies of this library. Include the appropriate [Containers Reflection](@ref
library_containers_reflection) serialization adapters when those owning types appear in the graph. Keeping the adapter
layer separate lets Serialization Text itself depend only on Reflection and lets applications provide the same traits
for their own vector-like types.

Writing walks fields in reflection visit order and appends compact JSON to a caller-selected growable buffer:

@snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonBasicWriteSnippet

The output is not null terminated. Use the buffer's actual size when constructing a view or writing it to a file.
`Options::floatDigits` controls the number of digits printed for floating-point values; pretty printing and other JSON
style controls are not currently exposed.

# Choose The Read Contract Deliberately

Both readers parse from an in-memory `StringSpan` and mutate an existing object. They are not streaming readers, and
they do not verify that no trailing input remains after the reflected value. They differ in how object fields are
located:

| Operation | Field contract | Intended use |
|-----------|----------------|--------------|
| `loadExact` | Names and order must match the writer's reflected order; whitespace may differ | Data produced and consumed by the same known schema |
| `loadVersioned` | Known fields may be reordered or omitted; matching is by reflected field name | Human-edited or schema-evolving text with controlled fields |

The versioned path leaves omitted fields at their current values. Initialize defaults before loading if that is the
desired migration behavior. Despite its name, it is not a permissive catch-all parser: an unknown object field makes
the load fail rather than being skipped. Fixed-size C arrays also retain exact array behavior.

This compiled test demonstrates reordered and missing fields; the missing `xy` field keeps the model's initialized
default:

@snippet Tests/Libraries/SerializationText/SerializationJsonTest.cpp serializationJsonBasicVersionedSnippet

`loadExact` has a simpler walk, but no measured performance claim is made for it. Select it for its stricter data
contract, then benchmark if speed matters.

# Storage, Allocation, And Lifetime

The serializer itself does not own a JSON document or build a tree. The important storage behavior belongs to the
objects supplied by the caller:

- `write` appends to the supplied buffer. A fixed-capacity buffer performs no heap allocation and returns `false` if
  it cannot grow; an owning growable buffer may allocate through its configured allocator.
- A failed write restores the destination buffer to its starting size. Existing bytes before that size are retained.
- Reading owning strings and vector-like containers may resize them and therefore may allocate. Resize failure makes
  the load fail. Loading is not transactional: do not assume the destination object is unchanged after failure.
- Loading into `StringSpan` or `StringView` borrows an unescaped slice of the input JSON. The input storage must outlive
  every resulting view. Escaped JSON strings cannot be represented by those borrowed types and cause loading to fail;
  use an owning `SC::String` when unescaping is required.
- The reader is whole-buffer rather than streaming. The input must remain available for the duration of parsing, and
  longer when the result contains borrowed string views.

These distinctions are how the library preserves caller control: "no mandatory allocation" does not mean that a
chosen owning output buffer or destination container can never allocate.

# Boundaries And Neighboring Libraries

[Reflection](@ref library_reflection) supplies field names and traversal metadata; Serialization Text supplies the JSON
reader and writer. [Containers Reflection](@ref library_containers_reflection) opts SC containers and owning memory
types into that traversal. [Serialization Binary](@ref library_serialization_binary) serves a different tradeoff: use
it when compact machine-oriented storage and its binary schema/versioning model matter more than readable JSON.

Internally, a stateful tokenizer returns slices into the source instead of constructing a DOM. It recognizes JSON
structure and token boundaries; type-specific readers then attempt the conversions they support. This keeps parsing
storage small, but `SC::JsonTokenizer` is an implementation-oriented surface, not a replacement for a validating JSON
document API.

Current practical boundaries include:

- JSON only; the shared traversal machinery could support another structured format, but XML and YAML are not
  implemented;
- no incremental input or output API;
- no unknown-field skipping in `loadVersioned`;
- no structured error object or byte position on failure;
- no JSON DOM and no preservation of formatting or key order from input;
- support for additional SC container families remains incomplete.

# API Reference

@copydoc SC::SerializationJson

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/SerializationText`.
Single File counts
`SaneCppSerializationText.h`.
Standalone counts `SaneCppSerializationTextStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 632		| 471		| 1103	|
| Single File | 1385		| 468		| 1853	|
| Standalone  | 2315		| 468		| 2783	|
