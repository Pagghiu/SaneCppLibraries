@page library_serialization_text Serialization Text

@brief 🟨 Serialize to / from text formats (JSON) using [Reflection](@ref library_reflection)

[TOC]

@copydetails group_serialization_text

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation), [Reflection](@ref library_reflection), [Strings](@ref library_strings)
- All dependencies: [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Reflection](@ref library_reflection), [Strings](@ref library_strings)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 309			| 136		| 445	|
| Sources   | 336			| 76		| 412	|
| Sum       | 645			| 212		| 857	|

# Features 
## JSON Serializer
- Serialize primitive types
- Serialize SC::Vector, SC::Array, SC::String
- Serialize `T[N]` arrays
- Serialize structs made of above types or other structs

# Status

🟨 MVP  
Only JSON has been implemented and it needs additional testing.

# JSON Serializer

@copydoc SC::SerializationJson

# Architecture

`SC::detail::SerializationTextReadVersioned` provides common framework for all text / structured formats, walking the data structure using reflection information.   
Every writer or reader needs to implement methods like `startObject` / `endObject` and `startArray` / `endArray`, so that only a minimal amount of work is needed to support a new output format.  
So far only JSON format has been implemented, but it would be easily possible adding XML or other formats like YAML if needed.  

Also this serializer has an **exact** and a **versioned** variant.  
The  **exact** json deserializer (`SC::detail::SerializationTextReadWriteExact`) must receive as input a file with fields in the exact same order as output by the json writer, so it makes it a little bit inflexible but maybe it could provide some performance boost as it's clearly doing less work (even if it should always be measured to verify it's actually faster in the specific use case).

## Json Tokenizer
`SC::JsonTokenizer` verifies the correctness of JSON elements but it will not validate Strings or parse numbers.
The design is a stateful streaming tokenizer, so it can ingest documents of arbitrary size.
It allocates just a single enum value for every _nesting level_ of json objects. 
This small allocation can be controlled  by the caller through a SC::Vector.
The allocation can be eliminated by passing a SC::SmallVector with an bounded level of json nesting.
The design of being a _tokenizer_ implies that we are not building any tree / hierarchy / DOM, even if it can be 
trivially done by just pushing the outputs of the tokenizer into a hierarchical data structure.

# Roadmap

🟩 Usable  
- JSON Escape Strings
- JSON UTF Escapes
- SC::ArenaMap serialization
- SC::SmallVector serialization
- SC::SmallString serialization

🟦 Complete Features:
- Streaming serializer

💡 Unplanned Features:
- XML Serializer
- YAML Serializer
