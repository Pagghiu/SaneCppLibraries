@page library_reflection_auto Reflection Auto

@brief 🟥 Describe C++ structs at compile time without listing all fields

[TOC]

Reflection Auto is a companion library to [Reflection](@ref library_reflection) allowing describing structures at compile time without explicitly listing all of its fields.

# Status

🟥 Draft  
This is mostly an experiment at this point, not even sure if this will be actually shippable anytime soon.  
Auto reflection is totally experimental and is not really production ready, as it fails on some types / compilers combinations too.


# Description
It's possible also trying the experimental **AUTO REFLECTION** mode.  
This mode is enabled the tests with the `SC_REFLECT_AUTOMATIC` macro set to 1.  
In this mode the reflection information are inferred automatically without needing to specialize `MetaClass` template.  
If code is being compiled with C++ 14 flags, the types reflected must support aggregate initialization, so this means that:
- C-Arrays are not handled properly (they collapse to a single element)
- Declaring a constructor on a struct will break the member detection

If code is compiled in C++ 20, another mechanism is used to detect fields (structured bindings), and so the previous limitations are lifted but a new one is added, that is:
- Cannot auto-deduce structs that inherit from other

@note Auto Reflection is probably not very useful as for any real world project data text-based file format will need the field names as string anyway. 
For binary file formats, their evolution will require manually matching field order between successive versions of the schema, and auto reflection can break file formats without even noticing by just moving, adding or removing a field from a struct already being serialized.

# Roadmap

🟦 Complete Features:
- To be decided

💡 Unplanned Features:
- None so far
