@page library_hashing Hashing

@brief 🟩 Compute `MD5`, `SHA1` or `SHA256` hashes for a stream of bytes

[TOC]

[SaneCppHashing.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppHashing.h) library abstracts OS API to compute MD5, SHA1 and SHA256 hashes.  

# Dependencies
- Dependencies: *(none)*
- All dependencies: *(none)*

![Dependency Graph](Hashing.svg)


# Features
| Hashing Algorithm         | Description                           |
|:--------------------------|:--------------------------------------|
| SC::Hashing::TypeMD5      | @copybrief SC::Hashing::TypeMD5       |     
| SC::Hashing::TypeSHA1     | @copybrief SC::Hashing::TypeSHA1      |     
| SC::Hashing::TypeSHA256   | @copybrief SC::Hashing::TypeSHA256    |

# Status
🟩 Usable  
The library is very simple it it has what is needed so far (mainly by [Build](@ref page_build)). 

# Description

@copydoc SC::Hashing

# Roadmap

🟦 Complete Features:
- None for now

💡 Unplanned Features:  
- None for now

# Statistics
LOC counts exclude comments. Library counts files physically under `Libraries/Hashing`.
Single File counts
`SaneCppHashing.h`.
Standalone counts `SaneCppHashingStandalone.h` and intentionally includes dependency
payloads.

| Metric      | Header | Source | Sum   |
|-------------|--------|--------|-------|
| Library     | 95		| 292		| 387	|
| Single File | 404		| 372		| 776	|
| Standalone  | 404		| 372		| 776	|
