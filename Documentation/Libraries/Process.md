@page library_process Process

@brief 🟩 Create child processes and chain them (also usable with [Async](@ref library_async) library)

[TOC]

Process allows launching and chaining input and output of child processes.

# Features
| Class                     | Description
|:--------------------------|:----------------------------------|
| SC::Process               | @copybrief SC::Process            |
| SC::ProcessChain          | @copybrief SC::ProcessChain       |

# Status
🟩 Usable  
Library allows all basic operations on child processes and it's being used in [Plugin](@ref library_plugin).

# Description

The SC::Process class is used when handling a process in isolation, while the SC::ProcessChain is used when there is need to chain inputs and outputs of multiple processes together.

## Process
@copydoc SC::Process

## ProcessChain
@copydoc SC::ProcessChain

# Roadmap

🟦 Complete Features:
- To be defined

💡 Unplanned Features:
- None so far
