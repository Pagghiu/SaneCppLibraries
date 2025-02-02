@page libraries_extra Libraries Extra

Some extra libraries are included in this repo even if they don't follow some of the [Principles](@ref page_principles).

Library                                             | Description
:---------------------------------------------------| :---------------------------------------------------
@subpage library_foundation_extra                   | @copybrief library_foundation_extra
@subpage library_reflection_auto                    | @copybrief library_reflection_auto
@subpage library_serialization_binary_type_erased   | @copybrief library_serialization_binary_type_erased

@note
- [Foundation Extra](@ref library_foundation_extra) are classes using some simple C++ meta-programming compared to the rest of the libraries.
- [Reflection Auto](@ref library_reflection_auto) doesn't fully comply to some project [Principles](@ref page_principles), due to use complex C++ meta-programming techniques.  
- [Serialization Type Erased](@ref library_serialization_binary_type_erased) is listed here as it's an alternative implementation.
