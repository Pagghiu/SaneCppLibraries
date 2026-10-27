# TOOLS-0014 - Share Test Expectation Literal Construction

Status: Accepted
Date: 2026-10-26

## Context

HttpClientTest.cpp took 110.7 seconds of overlapping task elapsed time in the hosted Fil-C x64 build.
An isolated pinned Fil-C ARM64 compile took 35.5 seconds versus 2.9 seconds with ordinary Clang using
the same Release flags. Fil-C spent 12.8 seconds in instrumentation and 23.6 seconds in the optimizer
overall. Preventing constructor scenario inlining did not improve it (36.5 seconds), so that probe was
rejected. Returning an expression StringSpan from a shared factory was also worse (45.5 seconds).

A helper sharing the complete literal-message construction and recorder call took 24.5 seconds in an
initial scratch probe. Repeating temporary StringSpan construction for every macro expansion creates
compiler work; sharing that code preserves its execution for every assertion.

## Decision

SC_TEST_EXPECT forwards its static expression literal, compile-time byte length, test object and status
to noinline TestCase templates for the existing bool and Result status forms. That helper constructs the same null-terminated ASCII StringSpan and
calls the concrete test object's existing public recordExpectation overload. Use the same value conversions as the recorder API, preserving bitfields and noncopyable values
convertible to bool; keep expression evaluation once and the existing debug-break branch. Do not bypass custom recorder
hooks, including AsyncExecutionParityTest's mutex. Add no allocation, dependency, system header or
instrumentation exemption. Leave the existing recorder overloads and failure formatting intact.

## Consequences

All assertions remain instrumented and execute the same recorder logic. One helper instantiation serves
many sites for a test/status type instead of duplicating message construction at each site. The explicit
noinline boundary adds a function call; measure test runtime as well as compiler time. It can benefit
ordinary compilers too, but Fil-C is the measured target. No compiler-cache or optimization-flag change
is part of this decision.

## Confirmation

Use paired old/new compiles with identical flags and private outputs; compare complete cold Fil-C builds
before attributing aggregate improvement. The focused TestingTest checks concrete recorder dispatch,
single evaluation, boolean outcomes, typed Result error identity and lvalue handling, bitfields, noncopyable boolean conversions and literal text,
encoding and null termination. Run the full Debug/Release matrix and generated standalone libraries.
Compare runtime and preserve every existing test section. Report cold compiler-cache invalidation
separately from structural compile gains when the shared testing header changes.

## Local result

The final bool/Result implementation passed identical-inventory cold Fil-C builds with 129 actions in
each: 177.843 to 154.752 seconds (23.091 seconds, 13.0%). Complete commands including equivalent
bootstrap invalidation took 183.143 to 159.987 seconds. All artifacts were preserved, the old macro
was used only for the control, and the candidate header was restored before runtime validation.

Full SCTest with --all-tests passed Debug and Release on macOS, Linux and Windows. The focused
regression passed in both configurations. Full stdexec execution suites passed Debug and Release on
all three platforms, including custom parity recorder hooks. Generated standalone libraries compiled
on all three platforms. Windows auxiliary validation uses the same native backend/vendor-header flags
as CI; an ad-hoc MSBuild auxiliary invocation encountered the existing external-warning override.

Hosted compilation and runtime comparisons must use the sequential bf86e1485 control; no hosted
speedup is established by the local result alone.
