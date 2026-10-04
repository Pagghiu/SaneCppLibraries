# FIBERS-0039 - Prototype Fil-C Contexts With Explicit Runtime-Owned Stacks

Status: Accepted
Date: 2026-10-04

## Context

Fil-C requires its garbage collector to know about suspended stacks. Native register-switch assembly does not
provide that integration. Its glibc distribution supports runtime fiber contexts, but pizfix exposes declarations
that abort at runtime. Supported contexts allocate hidden stacks and remain pinned to their creating thread.

## Decision

Prototype a separate internal context backend using Fil-C's supported runtime API. Require explicit
`createRuntimeOwned` creation and expose support through `supportsRuntimeOwnedStacks`. Keep caller-provided
stack creation unsupported on Fil-C; never silently substitute hidden allocations for caller storage.
Reject cross-thread checked switches before entering the runtime. Native compiler behavior remains unchanged.
Use glibc availability to distinguish the supported distribution from pizfix, rather than trusting declarations.

## Consequences

Runtime allocation is an explicit experimental exception, isolated from the existing scheduler API. This first
stage does not enable FiberScheduler or AsyncFibers, migrate contexts, implement guard pages, or claim that
caller-storage high-water measurements describe hidden stacks. Those need separate integration and tests.
Context lifetime follows Fil-C garbage collection; the runtime has no explicit context destruction API.
The low-level runtime header is internal upstream API, so this prototype is verified against Fil-C 0.685 and
must be revalidated on compiler upgrades. Thread-local storage is only a transient trampoline handoff;
entry/user data belong to the explicit context, not logical fiber state in thread-local storage.
CI keeps the rootless pizfix package and tests unsupported results until glibc packaging is integrated.

## Confirmation

Run the runtime-owned context section with both pizfix (unsupported) and glibc (actual suspend/resume and
wrong-thread rejection). Preserve native context and scheduler tests. No raw assembly fallback is allowed
on Fil-C.
