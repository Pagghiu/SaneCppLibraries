@page library_file_system_iterator File System Iterator

@brief 🟩 Enumerates files and directories inside a given path

[TOC]

[SaneCppFileSystemIterator.h](https://github.com/Pagghiu/SaneCppLibraries/releases/latest/download/SaneCppFileSystemIterator.h) is a library that enumerates files and directories at a given path.

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

# Statistics
| Type      | Lines Of Code | Comments  | Sum   |
|-----------|---------------|-----------|-------|
| Headers   | 78			| 72		| 150	|
| Sources   | 350			| 68		| 418	|
| Sum       | 428			| 140		| 568	|

# Features
- Iterate files in directory
- Handle recursive iteration

# Status
🟩 Usable  
The library is simple but gets the job done.  

- On Windows it expects paths in UTF8 or UTF16 and outputs paths as UTF16.
- On Posix it expects and outputs paths in UTF8.

# Blog

Some relevant blog posts are:

- [June 2025 Update](https://pagghiu.github.io/site/blog/2025-06-30-SaneCppLibrariesUpdate.html)
- [July 2025 Update](https://pagghiu.github.io/site/blog/2025-07-31-SaneCppLibrariesUpdate.html)

# Description
@copydetails SC::FileSystemIterator

# Roadmap

🟦 Complete Features:
- Not sure what else could be useful here

💡 Unplanned Features:
- No hypothesis has been made so far
