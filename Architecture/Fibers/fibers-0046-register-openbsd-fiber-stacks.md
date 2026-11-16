# FIBERS-0046 - Register OpenBSD fiber stacks

Status: Accepted
Date: 2026-11-16

## Context

OpenBSD requires the stack pointer at system-call entry to lie in a stack mapping. A context switch to ordinary
caller storage can perform user-space work and still receive SIGSEGV as soon as it calls into the kernel.

## Decision

Tag the pages owned by FiberVirtualStack and FiberStackClass with MAP_STACK. Reserve them inside the existing
mapping, then apply no-access protections and retain the normal guard and commit lifecycle. Leave stack-class
metadata in an ordinary mapping.

A raw span supplies exclusive scratch stack storage. Register the full pages inside that span using the native
sigaltstack remap, then use the registered interval for the context. Preserve the unused high-water prefix that
the remap zeroes. Temporarily mask signals during registration and restore the original mask. A thread with an
existing alternate signal stack can use FiberVirtualStack or FiberStackClass, which already have native stack
mappings.

## Confirmation

The existing FibersTest and AsyncFibersTest groups, including cross-thread resume and I/O, passed on OpenBSD 8.0
amd64 in Debug. The same FibersTest group passed in Debug and Release on macOS.
