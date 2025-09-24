@page library_reflection Reflection

@brief 🟩 Describe C++ types at compile time for serialization

[TOC]

@copydetails group_reflection

# Dependencies
- Direct dependencies: [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory)
- All dependencies: [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 684			| 310		| 994	|
| Sources   | 0			| 0		| 0	|
| Sum       | 684			| 310		| 994	|

# Features
- Reflection info is built at compile time
- Free of heap allocations
- Describe primitive types
- Describe C-Arrays
- Describe SC::Vector, SC::VectorMap, SC::Array, SC::String
- Describe Structs composition of any supported type
- Identify types that can be serialized with a single memcpy

# Status

🟩 Usable  
Under described limitations, the library should be usable.

# Roadmap

🟦 Complete Features:
- To be decided

💡 Unplanned Features:
- None so far

# Description
The main target use case is generating reflection information to be used for automatic serialization.
There are many rules and limitations so far, and the main one is not supporting any type of reference or pointer.
The output of the process is a *schema* of the reflected type.
This schema is an array of SC::Reflection::TypeInfo tracking the type and offset location (in bytes) of the field in the structure it belongs to.  
Fields that refer to non-primitive types (like other structs for example) can follow a *link* index that describes that field elsewhere in the scheme.

## Packed attribute
The schema contains information about all the types of all fields of the structure and the *packing state*.  
A *packed struct* is made of primitive types that are described to the Reflection system so that there are no padding bytes left in the struct.  
Example of packed struct: 
```cpp
struct Vec3
{
    float x;
    float y;
    float z;
};
```
Example of non-packed struct: 
```cpp
struct Vec3
{
    float x;
    uint16_t y; 
    // we have 4 bytes of padding between y and z
    float z;
};
```
A *recursively packed struct* is a struct made of other structs or arrays of structs without any padding bytes inside of themselves.
Example of recursively packed struct:
```cpp
struct ArrayOfVec3
{
    Vec3 array[10];
};

struct Vec3
{
    int32_t someNumber;
    ArrayOfVec3 array;
};
```

The *recursively packed* property allows binary serializers and deserializer to optimize reading / writing with a single `memcpy` (for example [Serialization Binary](@ref library_serialization_binary)).  

@note This means that serializers like [Serialization Binary](@ref library_serialization_binary) will not invoke type constructor when deserializing a Packed type, as all members are explicitly written by serialization.

## How to use it

Describing a structure is done externally to the struct itself, specializing a SC::Reflection::Reflect<> struct.  

For Example:
@snippet Tests/Libraries/Reflection/ReflectionTest.cpp reflectionSnippet1

## Struct member info
- These fields are required for binary and text serialization with **Versioning** support
    - `MemberTag` (integer)
    - `Pointer to Member`
    - `Field Name` (string)
    - `Field Byte Offset` in its parent struct
- This means being able to deserialize data from an older version of the program:
    - For **Binary Formats**: retaining data in struct members with matching `MemberTag`
    - For **Textual Formats**: retaining data in struct members with matching `Field Name`
    - Specifying both of them allow refactoring names of c++ struct members without breaking serialization formats
- The `Field Byte Offset` is necessary to generate an unique versioning signature of a given Struct
- The `Pointer to Member` allows serializing / deserializing without `reinterpret_cast<>` (we could use `Field Byte Offset` as an alternative)

@note Additional considerations regarding the level of *repetition*:
- There are techniques to get *field name as string* from member pointer on all compilers, but they're all C++ 20+.  
- There are techniques to get compile-time offset of field from member pointer but they are complex and increase compile time unnecessarily.
- We could hash the `Field Name` to obtain `MemberTag` but an explicit integer has been preferred to allow breaking textual formats and binary formats independently.

## Reflection Macros
With some handy macros one can save typing and they're generally preferable.

```cpp

SC_REFLECT_STRUCT_VISIT(TestNamespace::SimpleStructure)

SC_REFLECT_STRUCT_FIELD(0, f0)
SC_REFLECT_STRUCT_FIELD(1, f1)
SC_REFLECT_STRUCT_FIELD(2, f2)
SC_REFLECT_STRUCT_FIELD(3, f3)
SC_REFLECT_STRUCT_FIELD(4, f4)
SC_REFLECT_STRUCT_FIELD(5, f5)
SC_REFLECT_STRUCT_FIELD(6, f6)
SC_REFLECT_STRUCT_FIELD(7, f7)
SC_REFLECT_STRUCT_FIELD(8, f8)
SC_REFLECT_STRUCT_FIELD(9, f9)
SC_REFLECT_STRUCT_FIELD(10, arrayOfInt);

SC_REFLECT_STRUCT_LEAVE()
```

## Example (print schema)
To understand a little bit more how Serialization library can use this information, let's try to print the schema.  
The *compile time* flat schema can be obtained by calling SC::Reflection::Schema::compile:

```cpp
using namespace SC;
using namespace SC::Reflection;

constexpr auto SimpleStructureFlatSchema = Schema::compile<TestNamespace::SimpleStructure>();
```

For example we could print the schema with the following code:

@include Tests/Libraries/Reflection/ReflectionTestPrint.h

Called with the following code
```cpp
///....
printFlatSchema(report.console, SimpleStructureFlatSchema.typeInfos.values, SimpleStructureFlatSchema.typeNames.values);

```

It will print the following output for the above struct:

```
[00] TestNamespace::SimpleStructure (Struct with 11 members - Packed = false)
{
[01] Type=TypeUINT8   	Offset=0	Size=1	Name=f0
[02] Type=TypeUINT16  	Offset=2	Size=2	Name=f1
[03] Type=TypeUINT32  	Offset=4	Size=4	Name=f2
[04] Type=TypeUINT64  	Offset=8	Size=8	Name=f3
[05] Type=TypeINT8    	Offset=16	Size=1	Name=f4
[06] Type=TypeINT16   	Offset=18	Size=2	Name=f5
[07] Type=TypeINT32   	Offset=20	Size=4	Name=f6
[08] Type=TypeINT64   	Offset=24	Size=8	Name=f7
[09] Type=TypeFLOAT32 	Offset=32	Size=4	Name=f8
[10] Type=TypeDOUBLE64	Offset=40	Size=8	Name=f9
[11] Type=TypeArray   	Offset=48	Size=12	Name=arrayOfInt	[LinkIndex=12]
}
[12] Array (Array of size 3 with 1 children)
{
[13] Type=TypeINT32   	         	Size=4	Name=int
}
```
Another example with a more complex structure building on top of the simple one:

@snippet Tests/Libraries/Reflection/ReflectionTest.cpp reflectionSnippet2

Printing the schema of `ComplexStructure` outputs the following:

@note `Packed` structs will get their members sorted by `offsetInBytes`.  
For regular structs, they are left in the same order as the visit sequence.  
This allows some substantial simplifications in [Serialization Binary](@ref library_serialization_binary) implementation.

```
[00] TestNamespace::ComplexStructure (Struct with 6 members - Packed = false)
{
[01] Type=TypeUINT8   	Offset=0	Size=1	Name=f1
[02] Type=TypeStruct  	Offset=8	Size=64	Name=simpleStructure	[LinkIndex=7]
[03] Type=TypeStruct  	Offset=72	Size=64	Name=simpleStructure2	[LinkIndex=7]
[04] Type=TypeUINT16  	Offset=136	Size=2	Name=f4
[05] Type=TypeStruct  	Offset=144	Size=72	Name=intermediateStructure	[LinkIndex=19]
[06] Type=TypeVector  	Offset=216	Size=8	Name=vectorOfStructs	[LinkIndex=22]
}
[07] TestNamespace::SimpleStructure (Struct with 11 members - Packed = false)
{
[08] Type=TypeUINT8   	Offset=0	Size=1	Name=f0
[09] Type=TypeUINT16  	Offset=2	Size=2	Name=f1
[10] Type=TypeUINT32  	Offset=4	Size=4	Name=f2
[11] Type=TypeUINT64  	Offset=8	Size=8	Name=f3
[12] Type=TypeINT8    	Offset=16	Size=1	Name=f4
[13] Type=TypeINT16   	Offset=18	Size=2	Name=f5
[14] Type=TypeINT32   	Offset=20	Size=4	Name=f6
[15] Type=TypeINT64   	Offset=24	Size=8	Name=f7
[16] Type=TypeFLOAT32 	Offset=32	Size=4	Name=f8
[17] Type=TypeDOUBLE64	Offset=40	Size=8	Name=f9
[18] Type=TypeArray   	Offset=48	Size=12	Name=arrayOfInt	[LinkIndex=24]
}
[19] TestNamespace::IntermediateStructure (Struct with 2 members - Packed = false)
{
[20] Type=TypeVector  	Offset=0	Size=8	Name=vectorOfInt	[LinkIndex=26]
[21] Type=TypeStruct  	Offset=8	Size=64	Name=simpleStructure	[LinkIndex=7]
}
[22] SC::Vector (Vector with 1 children)
{
[23] Type=TypeStruct  	         	Size=64	Name=TestNamespace::SimpleStructure	[LinkIndex=7]
}
[24] Array (Array of size 3 with 1 children - Packed = true)
{
[25] Type=TypeINT32   	         	Size=4	Name=int
}
[26] SC::Vector (Vector with 1 children)
{
[27] Type=TypeINT32   	         	Size=4	Name=int
}
```

# Implementation
As already said in the introduction, effort has been put to keep the library as *readable* as possible, within the limits of C++.  
The only technique used is template partial specialization and some care in writing functions that are valid in `constexpr` context.  
For example generation of the schema is done through partial specialization of the SC::Reflection::Reflect template, by redefining the `visit` constexpr static member function for a given type.
Inside the visit function, it's possible to let the Reflection system *know* about a given field. 

The output of reflection is an array of SC::Reflection::TypeInfo referred to as **Flat Schema** at compile time.  
Such compile time information is used when serializing and deserializing data that has missing fields.

@snippet Libraries/Reflection/Reflection.h reflectionSnippet3

@snippet Libraries/Reflection/Reflection.h reflectionSnippet4

The flat schema is generated by SC::Reflection::SchemaCompiler, that walks the structures and building an array of SC::Reflection::TypeInfo describing each field.  

- Primitive types just need a single SC::Reflection::TypeInfo.  
- Complex types are described using multiple SC::Reflection::TypeInfo.  

For example a struct is defined by a SC::Reflection::TypeInfo that contains information on the `numberOfChildren` of the struct, corresponding to the number of fields. `numberOfChildren` SC::Reflection::TypeInfo exist immediately after the struct itself in the flat schema array.  
If type of one of these fields is complex (for example it refers to another struct) it contains a  *link*.  
A Link is an index/offset in the flat schema array (`linkIndex` field).  

This simple data structure allows to describe hierarchically all types in a structure decomposing it into its primitive types or into special classes that must be handled specifically (SC::Vector, SC::Array etc.).  
It also has an additional nice property that it's trivially serializable by dumping this (compile time known) array with a single `memcpy` or by calculating an `hash` that can act as a unique signature of the entire type itself (for versioning purposes).  

It's possible associating a `string` literal with each type or member, so that text based serializer can refer to it, as used by the JSON [Serialization Text](@ref library_serialization_text).  
Lastly it's also possible associating to a field an `MemberTag` integer field, that can be leveraged by by binary serializers to keep track of the field position in binary formats. This allows binary formats to avoid using strings at all for format versioning/evolution, allowing some executable size benefits. This makes also possible not breaking binary file formats just because a field / member was renamed. This is leveraged by the [Serialization Binary](@ref library_serialization_binary).

@note It's possible also trying the experimental `SC_REFLECT_AUTOMATIC` mode by using [Reflection Auto](@ref library_reflection_auto) library that automatically lists struct members. This makes sense only if used with [Serialization Binary](@ref library_serialization_binary), as `SC_REFLECT_AUTOMATIC` cannot obtain field names as strings, so any text based serialization format like [Serialization Text](@ref library_serialization_text) cannot work.  
[Reflection Auto](@ref library_reflection_auto) library is an experimental library, unfortunately using some more obscure C++ meta-programming techniques, part of [Libraries Extra](@ref libraries_extra).