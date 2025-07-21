@page library_file_system_iterator File System Iterator

@brief 🟩 Enumerates files and directories inside a given path

[TOC]

SC::FileSystemIterator enumerates files and directories at a given path.

# Dependencies
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)

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

# Description
@copydetails SC::FileSystemIterator

# Roadmap

🟦 Complete Features:
- Not sure what else could be useful here

💡 Unplanned Features:
- No hypothesis has been made so far
