# SC-0016 - Support Layered Adoption Modes

Status: Accepted
Date: 2026-07-02

## Context

Sane C++ Libraries serves different users. Some want one drop-in platform abstraction library, some want the whole library set, and some want the complete self-hosted build, package, project-generation, and toolchain workflow. Optimizing only for the full environment would make single-library consumption fragile. Optimizing only for drop-in headers would weaken repository tooling and validation.

## Decision

The project supports layered adoption modes. Individual libraries must remain consumable through single-file outputs where practical. The full repository can be consumed through `SC.cpp`. `SC::Build`, `SC::Package`, bootstrap tools, generated projects, documentation, coverage, package downloads, and toolchain handling are optional development-environment layers, not prerequisites for using the libraries.

## Consequences

The full train can become powerful without becoming mandatory. Library APIs, dependency shape, public headers, and single-file generation must continue to protect lower adoption layers. Tooling may provide a richer self-contained workflow, but it must not leak into normal library consumption as a required dependency.

## Confirmation

A change preserves this decision when single-file libraries still compile, repo-level `SC.cpp` integration still works, build and package tooling features remain opt-in, and normal library users are not forced to adopt `SC::Build` or downloaded toolchains.

## Related

- [README usage options](../../README.md#how-to-use-sane-c-libraries-in-your-project)
- [Building user documentation](../../Documentation/Pages/BuildingUser.md)
- [SC::Build documentation](../../Documentation/Pages/Build.md)
- [SC-0004 - Single-file libraries are first-class distribution artifacts](sc-0004-single-file-libraries-are-first-class-distribution-artifacts.md)
