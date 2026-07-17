@page page_building_contributor Building (Contributor)

Contributor builds are organized around a short feedback loop: compile the affected target, run the narrowest relevant
test, then expand validation before committing. Project generation is optional.

[TOC]

# Prepare the checkout

Use a recent C++ compiler supported by the host platform and make sure Git subcommands and the repository bootstrap can
run. The first tool invocation compiles the small bootstrap and may download pinned development tools into `_Build`.

Read [CONTRIBUTING.md](https://github.com/Pagghiu/SaneCppLibraries/blob/main/CONTRIBUTING.md) before changing code. It
defines the issue-first workflow, commit format, and agent guidance. Read [Coding Style](@ref page_coding_style) before
writing a new API.

# Use the focused feedback loop

Compile the test executable before running it:

```bash
./SC.sh build compile SCTest Debug
./SC.sh build run SCTest Debug -- --test "StringsTest"
```

Replace `StringsTest` with the test relevant to the change. To isolate one named section:

```bash
./SC.sh build run SCTest Debug -- --test "HttpClientTest" --test-section "basic GET"
```

Arguments after `--` belong to the test executable. Arguments before it belong to `SC::Build`.

Network tests can run alongside other worktrees by adding a unique `--port-offset`. A hang usually means an async
handle, task, or reference count was not closed; reduce the run to one section before adding temporary tracing.

# Expand validation deliberately

After focused tests pass:

1. Compile and run `SCTest` in Debug.
2. Repeat in Release when the change affects library behavior, templates, assertions, or optimization-sensitive code.
3. Compile the relevant examples or tools.
4. Compile `SCSingleFileLibs:` when library dependencies or public headers changed.
5. Test the other supported operating systems when the implementation or build logic is platform-specific.

Representative commands:

```bash
./SC.sh build compile SCTest Release
./SC.sh build run SCTest Release
./SC.sh build compile "SCSingleFileLibs:" Release
```

`BuildTest` is omitted from ordinary local `SCTest` runs because it exercises broader toolchain flows. Use `--all-tests`
when the change touches SC::Build, package resolution, bootstrap behavior, or CI-equivalent validation:

```bash
./SC.sh build run SCTest Debug -- --all-tests
```

# Format and inspect the change

Format supported source files with the pinned formatter:

```bash
./SC.sh format execute
```

Then inspect the diff. Formatting is necessary but does not catch accidental generated files, over-wide declaration
alignment, stale comments, or unrelated edits.

# Generate IDE projects only when useful

The native backend is the default command-line workflow. Generate projects when you want an IDE's project model or a
generated Make flow:

```bash
./SC.sh build configure SCTest
```

Generated projects are placed below `_Build/_Projects`. They are disposable and should not become the source of truth
for target definitions; edit `Tools/SC-build.cpp` instead.

# Debug a failing test

Build Debug, reduce the test selection, and launch the produced executable through LLDB, GDB, Visual Studio, or the
repository's VS Code configuration. The output directory printed by SC::Build identifies the exact executable for the
selected platform, architecture, toolchain, and configuration.

When debugging async code, reconstruct ownership and completion in order:

1. Which operation acquired a reference or task slot?
2. Which callback or coroutine releases it?
3. Which close operation allows the event loop to finish?
4. Does every error path perform the same cleanup?

Remove temporary logging after the behavior is understood.

# Build documentation

Documentation is part of validation when changing public headers or Markdown pages:

```bash
./SC.sh build documentation
```

Doxygen warnings fail the build. Check the generated page as well as the source because snippets, links, tables, and
embedded HTML are transformed during generation.
