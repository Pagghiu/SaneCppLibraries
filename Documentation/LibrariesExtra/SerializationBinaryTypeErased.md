@page library_serialization_binary_type_erased Serialization Binary Type Erased

@brief 🟥 Implementation of [SerializationBinary](@ref library_serialization_binary) using a different approach.

[TOC]

Implementation of [SerializationBinary](@ref library_serialization_binary) using a different approach.

# Features
Same as [SerializationBinary](@ref library_serialization_binary)

# Status

🟥 Draft  
Under described limitations, the library should be usable but for now we consider it just a test.

# Description
[SerializationBinary](@ref library_serialization_binary) serializer is defined 100% in headers using template specialization to generate serialization code from the compile time type info generated by [Reflection](@ref library_reflection).  
This serializer generates probably faster and more specialized code but might have more impact on compile time.  

[SerializationBinaryTypeErased](@ref library_serialization_binary_type_erased) serializer could be less impactful on compile time as it's walking the type infos array at runtime.  
This serializer instead is mostly defined in `.cpp` files and walks the type info array at runtime.

Compile time performances are mainly speculation as there is no actual benchmark proving that.
We should really measure actual compile times on the same set of reflected types, but so far it has not been done yet.

# Roadmap
None. This is just a test for now.

🟦 Complete Features:
- None so far

💡 Unplanned Features:
- None so far