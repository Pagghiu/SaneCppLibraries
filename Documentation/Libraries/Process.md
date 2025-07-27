@page library_process Process

@brief 🟩 Create child processes and chain them (also usable with [Async](@ref library_async) library)

[TOC]

Process allows launching, chaining input and output, setting working directory and environment variables of child processes.

# Quick Sheet

\snippet Tests/Libraries/Process/ProcessTest.cpp ProcessQuickSheetSnippet

# Dependencies
- Direct dependencies: [File](@ref library_file), [Foundation](@ref library_foundation)
- All dependencies: [File](@ref library_file), [Foundation](@ref library_foundation)

# Statistics
- Lines of code (excluding comments): 1318
- Lines of code (including comments): 1828

# Features
| Class                     | Description
|:--------------------------|:----------------------------------|
| SC::Process               | @copybrief SC::Process            |
| SC::ProcessChain          | @copybrief SC::ProcessChain       |
| SC::ProcessEnvironment    | @copybrief SC::ProcessEnvironment |
| SC::ProcessFork           | @copybrief SC::ProcessFork        |

# Status
🟩 Usable  
Library is being used in [SC::Plugin](@ref library_plugin) and in [SC::Tools](@ref page_tools).

# Description

The SC::Process class is used when handling a process in isolation, while the SC::ProcessChain is used when there is need to chain inputs and outputs of multiple processes together.

# Videos

This is the list of videos that have been recorded showing some of the internal thoughts that have been going into this library:

- [Ep.06 - Posix fork](https://www.youtube.com/watch?v=-OiVELMxL6Q)

# Blog

Some relevant blog posts are:

- [March 2024 Update](https://pagghiu.github.io/site/blog/2024-03-27-SaneCppLibrariesUpdate.html)
- [April 2024 Update](https://pagghiu.github.io/site/blog/2024-04-27-SaneCppLibrariesUpdate.html)
- [April 2025 Update](https://pagghiu.github.io/site/blog/2025-04-30-SaneCppLibrariesUpdate.html)

## Process
@copydoc SC::Process

## ProcessChain
@copydoc SC::ProcessChain

## ProcessEnvironment
@copydoc SC::ProcessEnvironment

## ProcessFork
@copydoc SC::ProcessFork

# Roadmap

🟦 Complete Features:
- To be defined

💡 Unplanned Features:
- None so far
