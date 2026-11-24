# TOOLS-0026 - Export symbols from native BSD hosts

Status: Accepted
Date: 2026-11-03

## Context

Native BSD builds initially rejected projects that select host exports. Source-compiled plugins may call
functions provided by their host, including the Memory API used by the plugin linker definitions. Building and loading a
plugin that does not call a host function does not establish that this integration works.

## Decision

Allow library and source-directory exports for native BSD executables. Export visible executable symbols dynamically, and
when Release dead-code stripping and export preservation are enabled, generate the same symbol and ELF dynamic lists used
for native Linux. Use the native `nm` and `objcopy` tools to preserve selected symbols. The tested FreeBSD linker does not
provide GNU `--gc-keep-exported`; its dynamic exports are retained using `-rdynamic` with the dynamic list.

Native OpenBSD uses the same dynamic list and strip flow with its base tools. Read names from `nm -gP` and filter
out undefined symbols and file records because the base `nm` lacks the GNU defined-only and names-only switches.
See the [OpenBSD nm manual](https://man.openbsd.org/OpenBSD-8.0/nm.1).

Apply the executable dynamic-export flag to directory-only native Linux projects as well as projects that export Sane
libraries. Generated BSD backends and cross-target toolchains remain outside this decision.

## Consequences

On the tested NetBSD GCC, FreeBSD Clang, and OpenBSD Clang toolchains, plugins can resolve the selected host API in Debug
and after Release stripping. This does not qualify other BSD toolchains, plugin ABI combinations, or watcher-driven
reload. Callers still opt into the host exports their plugins require.

## Confirmation

Build and run the shared Plugin compiler smoke test against a host that exports Memory on all three BSDs. Also run the
native source-directory export integration fixture on NetBSD, FreeBSD, OpenBSD, and Linux in Debug and Release; it loads a
separate plugin that calls an otherwise unreferenced exported host function and checks that a hidden host function is not
dynamically visible.

## Related

- [TOOLS-0025 - Build native BSD targets with distinct identities](tools-0025-build-native-bsd-targets.md)
- [PLUGIN-0003 - Make plugin runtime and sysroot policy explicit](../Plugin/plugin-0003-make-plugin-runtime-and-sysroot-policy-explicit.md)
