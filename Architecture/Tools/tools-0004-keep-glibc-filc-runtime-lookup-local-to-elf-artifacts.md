# TOOLS-0004 - Keep Glibc Fil-C Runtime Lookup Local To ELF Artifacts

Status: Accepted
Date: 2026-10-07

## Context

The experimental glibc Fil-C profile requires libraries under `/opt/fil/lib`. Injecting this directory
through LD_LIBRARY_PATH also exposes those libraries to native children. ProcessTest's native grep
pipeline fails because it resolves the Fil-C PCRE2 library instead of the host library.

## Decision

Stop injecting `/opt/fil/lib` into the launch environment. Preserve the compiler-selected Fil-C interpreter,
whose system search path already contains that directory, and existing Process environment inheritance.
Do not add redundant RPATH/RUNPATH flags, strip user-provided loader variables, or infer child ABI from its name.

## Consequences

Interpreter-local runtime lookup no longer contaminates native subprocesses. User loader overrides remain
inherited, and dynamically loaded libraries use the Fil-C loader's normal search policy. The profile remains
tied to its preinstalled `/opt/fil` distribution. Rootless package-local library lookup is unchanged.

## Confirmation

Inspect the linked artifact's interpreter and that loader's system search path. Validate startup, ProcessTest self-exec and native
pipelines, PluginTest compiler subprocesses and loading, and runtime-owned Fibers/AsyncFibers in Debug
and Release without a launcher-injected LD_LIBRARY_PATH. CI runs full ProcessTest rather than a signal-only
section. Native and rootless profiles must retain their existing launch behavior.

## Related

- [Tool documentation](../../Documentation/Pages/Tools.md)
- [Linux dynamic loader search rules](https://man7.org/linux/man-pages/man8/ld.so.8.html)
- [Fil-C plugin toolchain isolation](../Plugin/plugin-0005-isolate-filc-plugin-toolchains-and-document-retained-modules.md)
