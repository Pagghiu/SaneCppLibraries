@page library_containers_serialization Containers Serialization

@brief 🟨 Containers specializations for [Reflection](@ref library_reflection) and Serialization.

[TOC]

This is a support library holding all partial specializations for [Reflection](@ref library_reflection), [Serialization Binary](@ref library_serialization_binary) and [Serialization Text](@ref library_serialization_text).  
Its headers are only meant to be included anywhere the reflection / serialization systems are being used so that both systems "know" how to handle things like `SC::Vector` or `SC::String`.

@note The reason for this library to exist is only to allow [Reflection](@ref library_reflection), [Serialization Binary](@ref library_serialization_binary) and [Serialization Text](@ref library_serialization_text) libraries not to be depending on [Containers](@ref library_containers) and [Memory](@ref library_memory).  

# Dependencies
- Dependencies: [Containers](@ref library_containers), [Reflection](@ref library_reflection)
- All dependencies: [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Reflection](@ref library_reflection)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 97			| 129		| 226	|
| Sources   | 639			| 106		| 745	|
| Sum       | 736			| 235		| 971	|
