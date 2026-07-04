# PLUGIN-0001 - Compile Source-Defined Plugins On Demand and Load Them In-Process

Status: Accepted
Date: 2026-07-04

## Context

Plugin exists to support C++ hot reload and host/plugin contracts without requiring users to adopt a separate build system. The useful workflow is powerful but risky: compiling source at runtime and loading native dynamic libraries into the host process can crash the host if plugin code or shutdown behavior is wrong.

## Decision

Plugins are source-defined. A plugin declares metadata in a source comment block, `PluginScanner` parses definitions, `PluginCompiler` compiles and links source files, and `PluginRegistry` loads the resulting native dynamic libraries in the current process. Runtime contracts use exported entry points and `queryInterface`-style interface lookup.

## Consequences

Hot reload stays simple and native for desktop development and examples. The model intentionally favors source-delivered plugins over prebuilt binary plugin distribution. Safety, crash isolation, and hostile-plugin sandboxing are out of scope for this in-process model unless a future ADR adds another execution mode.

## Confirmation

A change preserves this decision when plugin loading still starts from source definitions, compile/link/load/reload behavior is exercised through Plugin tests or examples, dynamic-library entry points remain explicit, and new plugin execution modes do not replace the in-process source-defined path without a superseding ADR.

## Related

- [Plugin documentation](../../Documentation/Libraries/Plugin.md)
- [Plugin public API](../../Libraries/Plugin/Plugin.h)
- [Plugin tests](../../Tests/Libraries/Plugin/PluginTest.cpp)
- [SC-0008 - Prefer native OS APIs over third-party dependencies](../Global/sc-0008-prefer-native-os-apis-over-third-party-dependencies.md)
- [SC-0017 - Publish Draft and MVP libraries deliberately](../Global/sc-0017-publish-draft-and-mvp-libraries-deliberately.md)
