# TOOLS-0012 - Reuse a Compatible MSVC Environment in the PowerShell Launcher

Status: Accepted
Date: 2026-10-26

## Context

SC-build.ps1 calls vcvarsall before both rebuilding the C bootstrap and invoking it. This repeats
initialization even when SC.bat already initialized the parent fixture process. A Windows VM probe
measured repeated initialization at 1.62, 1.61 and 1.72 seconds, despite an initialized environment.
This qualifies the runtime-fixture observation in TOOLS-0010: inheritance alone does not remove this cost.

## Decision

Keep latest-vswhere discovery and default launcher behavior. Opt in only the Windows build-fixture CI
job through SC_BUILD_REUSE_MSVC_ENVIRONMENT=1. Reuse the environment only when VSCMD_VER is present, the compiler host
is x86, the target is x64, and VSINSTALLDIR matches that selected installation, ignoring case and trailing
backslashes. Otherwise retain the original vcvarsall x86_amd64 command. Apply this guard to both C
bootstrap compilation and invocation, preserving all compiler/linker flags and forwarded arguments.

Prefix reused CMD commands with @ so a quoted executable path is not the first character interpreted
by CMD /c. Do not introduce CALL around the executable, which would add another argument expansion.

## Consequences

Compatible fixture calls avoid redundant setup without sharing compiled artifacts or changing source,
SDK, compiler host, target, test coverage, cache invalidation, or timeout policy. Cold and incompatible
environments still exercise discovery and initialization. The explicit opt-in prevents changing normal developer invocations that may use a custom SDK or toolset.
The CI parent initializes the default compiler/SDK through SC.bat. Do not enable reuse in a job that
requires resetting an inherited custom SDK/toolset to Visual Studio defaults.

## Confirmation

Check compatible, missing-marker, wrong-host, wrong-target, and different-installation environments;
exercise quoted executable paths, empty arguments, spaces and CMD special characters. Run existing
Windows Debug/Release build fixtures, including cold bootstrap, invalidation, Unicode and deep paths.
Compare paired real warm compile calls with the preceding launcher; measure hosted fixture steps
before claiming an end-to-end CI saving.

Paired warm external native compile calls on one Windows VM measured the preceding launcher at
3.906/3.942/3.790 seconds and reuse at 1.978/2.057/2.033 seconds. Median saving: 1.873 seconds per
call (48%). Both launchers reported all three build actions skipped. Exclude the first build/warmup
pair from these numbers; no hosted workflow saving is established by this local result.
