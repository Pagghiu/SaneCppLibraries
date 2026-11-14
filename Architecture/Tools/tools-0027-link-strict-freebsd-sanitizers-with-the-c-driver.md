# TOOLS-0027 - Link Strict FreeBSD Sanitizers with the C Driver

Status: Accepted
Date: 2026-11-10

## Context

The native FreeBSD Clang C++ link driver adds the C++ sanitizer archive even with `-nostdlib++`. The
archive needs C++ ABI type information that strict Sane C++ projects intentionally omit.
On the tested FreeBSD 15.1 guest, concurrent fork and ASan thread startup can deadlock inside the
runtime loader and sanitizer allocator.

## Decision

For native FreeBSD Clang links with sanitizers enabled and `linkStdCpp` disabled, select the C driver
when the configured linker is the default C++ driver. The C driver links the ASan and UBSan
instrumentation used by these projects and leaves the C++ runtime out. Keep native FreeBSD Debug
sanitizers opt-in until the base runtime is qualified for concurrent fork and thread startup.

## Confirmation

Link and run a strict FreeBSD target with sanitizers explicitly enabled. Check its dynamic dependencies
and confirm that intentional heap bounds and signed overflow violations produce sanitizer diagnostics.
Run process and thread coverage through the default native Debug preset.

## Related

- [TOOLS-0025](tools-0025-build-native-bsd-targets.md)
