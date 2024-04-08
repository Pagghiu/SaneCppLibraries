@page library_process Process

@brief 🟩 Create child processes and chain them (also usable with [Async](@ref library_async) library)

[TOC]

Process allows launching, chaining input and output, setting working directory and environment variables of child processes.

# Features
| Class                     | Description
|:--------------------------|:----------------------------------|
| SC::Process               | @copybrief SC::Process            |
| SC::ProcessChain          | @copybrief SC::ProcessChain       |
| SC::ProcessEnvironment    | @copybrief SC::ProcessEnvironment |

# Status
🟩 Usable  
Library is being used in [SC::Plugin](@ref library_plugin) and in [SC::Tools](@ref page_tools).

# Description

The SC::Process class is used when handling a process in isolation, while the SC::ProcessChain is used when there is need to chain inputs and outputs of multiple processes together.

## Process
@copydoc SC::Process

## ProcessChain
@copydoc SC::ProcessChain

## ProcessEnvironment
@copydoc SC::ProcessEnvironment

# Roadmap

🟦 Complete Features:
- To be defined

💡 Unplanned Features:
- None so far
