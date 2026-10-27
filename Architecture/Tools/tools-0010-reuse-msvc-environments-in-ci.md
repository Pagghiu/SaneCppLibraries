# TOOLS-0010 - Reuse MSVC Environments in CI

Status: Accepted
Date: 2026-10-26

## Context

SC.bat initializes Visual Studio through vswhere and vcvarsall x86_amd64 when VSCMD_VER is unset.
Each CI command runs in a new process, so repeated calls repeat that setup. Runtime build fixtures already
inherit their parent SC.bat environment; their separate compiler work is not an environment-setup saving.

A paired Windows VM probe using the existing header/no-runtime target measured no-op build calls at
4.016/4.031/3.891 seconds without an initialized environment and1.672/1.797/1.671 seconds with one.
The median difference is2.344 seconds per invocation. Initializing and serializing the environment cost
5.000 seconds in that probe, making jobs with at least three invocations the candidates for amortization.

## Decision

Initialize MSVC once immediately before the first SC.bat step in package-warmup, core and auxiliary Windows jobs, each of which
performs at least four SC.bat invocations. Leave the two-call build-fixture jobs on the original cold path,
retaining real SC.bat discovery/bootstrap coverage on both Windows images without adding setup overhead.
Do not change SC.bat, compiler/target selection, flags, test commands, caches, retries or timeouts.

Use the same latest Visual Studio selection, x86_amd64 environment and telemetry settings as SC.bat.
Capture the initialized environment as JSON through the runner's existing Python, and persist only changed
variables through UTF-8 GITHUB_ENV records. Preserve the complete PATH value/order and multiline/Unicode
values. Reject unexpected protected-variable changes; do not print environment values. If VSCMD_VER was
already present, reuse the existing environment, matching SC.bat's behavior.

## Confirmation

The local export/import round trip matches an independent cold vcvarsall environment for PATH, INCLUDE,
LIB, LIBPATH, compiler/SDK roots and host/target architecture. The compiler path is identical, unrelated
Unicode/multiline variables remain intact, and repeated initialization exports no additional state.

Hosted validation must pass the existing Debug/Release and runtime fixture matrix. Compare the total of
initializer plus original steps, job execution seconds and workflow elapsed against the preceding exact run.
Report runner/toolchain variance and cold-path checks; do not count the per-call saving without setup cost.

## Hosted result

At d7d223ddf all Windows jobs passed. Setup cost3–5seconds per selected job. The first comparison against
899c095da measured261 ->280seconds workflow elapsed and1788 ->1792summed runner-seconds. It does not
establish an end-to-end improvement: Release core jobs were51–66seconds faster, while auxiliary jobs and
unchanged cold fixture controls also varied materially. The controlled per-call saving supports removing
repeated setup, but its predicted contribution must not be confused with those larger runner/LTCG differences.
