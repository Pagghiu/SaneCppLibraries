# Sane C++ Libraries

[TOC]
**Sane C++ Libraries** is a set of C++ platform abstraction libraries for macOS, Windows and Linux. [Platforms](@ref page_platforms).  

Project [Principles](@ref page_principles):

@copybrief page_principles

## Motivation

- Having fun building from scratch a cohesive ecosystem of libraries sharing the same core principles
- Fight bloat measured in cognitive and build complexity, compile time, binary size and debug performance
- Providing out-of-the-box functionalities typically given for granted in every respectable modern language
- [Re-invent the wheel](https://xkcd.com/927/) hoping it will be more round this time

You can take a look at the [introductory blog post](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html) if you like.

I've also started a [Youtube Channel](https://www.youtube.com/@Pagghiu) with some videos on the project.

## Status
Many libraries are in draft state, while others are slightly more usable.  
Click on specific page each library to know about its status.  

- 🟥 Draft (incomplete, work in progress, proof of concept, works on basic case)
- 🟨 MVP (minimum set of features have been implemented)
- 🟩 Usable (a reasonable set of features has been implemented to make library useful)
- 🟦 Complete (all planned features have been implemented)

It is a deliberate decision to prototype single libraries and make them public Draft or MVP state.  
This is done so that they can be matured in parallel with all other libraries and evolve their API more naturally.  

@copydetails libraries
