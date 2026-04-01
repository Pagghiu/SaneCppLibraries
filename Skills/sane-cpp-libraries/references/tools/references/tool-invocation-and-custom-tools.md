# Tool Invocation And Custom Tools

Use this reference when a user needs to understand `SC.sh`, `SC.bat`, built-in tools, or a custom tool `.cpp` file.

## What This System Does

- Compile small C++ tools on demand.
- Run those tools immediately after compilation.
- Reuse the same bootstrap interface for built-in and custom tools.

## Invocation Model

1. The user calls `SC.sh` or `SC.bat`.
2. The first token selects the tool, such as `build`, `package`, or `format`.
3. The next token selects the action.
4. Remaining arguments are forwarded to the tool.

## Useful Repository Paths

- `Documentation/Pages/Tools.md`
- `Tools/SC-build.cpp`
- `Tools/Tools.cpp`
- `Tools/SC-package.cpp`
- `Tools/SC-format.cpp`

## Custom Tool Guidance

- Put the tool logic in a standalone `.cpp` file.
- Let the bootstrap compile `Tools/Tools.cpp` plus the requested tool file.
- Use Sane C++ libraries directly inside the tool.

## Pitfalls

- Do not present SC::Tools as a separate build system.
- Do not forget that built-in tools are still ordinary C++ programs.
- Keep command examples aligned with the bootstrap/action/argument model.
