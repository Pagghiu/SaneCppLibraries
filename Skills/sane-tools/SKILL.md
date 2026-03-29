---
name: sane-tools
description: SC::Tools invocation guidance for third-party AI agents using Sane C++ Libraries. Use when explaining SC.sh or SC.bat, built-in tool actions, custom tool files, or the bootstrap and compile-on-demand workflow.
---

# Sane Tools

## Quick Start

- Use this skill when you need to explain `SC.sh`, `SC.bat`, built-in tools, or custom tool invocation.
- Start with [tool-invocation-and-custom-tools.md](references/tool-invocation-and-custom-tools.md).

## Core Workflow

1. Read `Documentation/Pages/Tools.md` for the bootstrap model.
2. Identify whether the user is calling a built-in tool such as `build`, `package`, or `format`.
3. If the user wants a custom tool, explain how the `.cpp` file is compiled on demand and then executed.
4. Route build-specific questions to `sane-build` when the user is actually configuring `SC::Build`.

## What To Emphasize

- Tools are small C++ programs compiled on the fly.
- `SC.sh` and `SC.bat` are the user-facing entrypoints.
- The first argument after the tool name is the action, and later arguments are forwarded.
- `Tools/Tools.cpp` and `Tools/SC-build.cpp` are the key internal examples.

## Common Pitfalls

- Do not describe `SC::Tools` as a separate scripting language.
- Do not hide the bootstrap sequence when the user asks how invocation works.
- Keep tool-adoption guidance separate from build-system guidance.

## References

- [tool-invocation-and-custom-tools.md](references/tool-invocation-and-custom-tools.md)
- `Documentation/Pages/Tools.md`
- `Tools/SC-build.cpp`
