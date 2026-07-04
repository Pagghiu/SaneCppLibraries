# SERIALIZATIONTEXT-0004 - Tokenize JSON Without Building a DOM

Status: Accepted
Date: 2026-07-04

## Context

Serialization Text needs to parse JSON input for reflected objects without hidden allocation. A DOM would require an owned tree, allocation policy, and additional lifetime rules that are unnecessary for streaming reflected deserialization.

## Decision

The JSON support in Serialization Text uses `JsonTokenizer` as a cursor-based tokenizer. It returns token types and slices into the original input, does not build a DOM, and deliberately leaves full string and number validation to the reader layer that consumes the tokens.

## Consequences

JSON reads can operate over caller-owned text without allocating a parse tree. The tokenizer stays small and constexpr-friendly, but consumers must perform semantic validation, number parsing, and string unescaping where needed. Future dynamic JSON document support belongs to the separate JSON library scope, not this SerializationText decision.

## Confirmation

A change preserves this decision when `JsonTokenizer` remains a token stream over `StringSpan`, tokens refer back to input slices rather than owned nodes, Serialization Text JSON reads do not construct a hierarchy, and dynamic JSON document work is kept out of the `SERIALIZATIONTEXT` scope.

## Related

- [JsonTokenizer](../../Libraries/SerializationText/Internal/JsonTokenizer.h)
- [Serialization Text documentation: Json Tokenizer](../../Documentation/Libraries/SerializationText.md)
- [JsonTokenizer tests](../../Tests/Libraries/SerializationText/JsonTokenizerTest.cpp)
- [SC-0001 - Library code must not hide dynamic allocation](../Global/sc-0001-no-hidden-allocation.md)
