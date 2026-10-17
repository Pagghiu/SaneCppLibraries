# ASYNCEXECUTION-0003 - Compile Non-Template Runtime Once

Status: Accepted
Date: 2026-10-17

## Context

The feasibility implementation placed dispatcher, allocation, file operations and runtime lifecycle bodies in AsyncExecution.h. Each consumer parsed those bodies and instantiated common pool-ticket code for its receiver type. The user requested the usual Sane C++ Libraries header/implementation split to reduce repeated compilation.

## Decision

Compile non-template runtime code in AsyncExecution.cpp. Keep public layouts, sender/receiver templates, completion signatures and small accessors in the header. Give non-template sender factories explicit return types. Use a named dependent-send continuation so sendTCP also has an explicit composed sender type and an out-of-line body, preserving its standard let_value composition without additional type erasure or allocation.

Move pool-ticket allocation, cleanup and task submission into one non-template helper. Only receiver delivery remains templated, invoked through a non-owning function pointer while the ticket preserves the existing operation-destruction and worker-join ordering. Keep C allocation functions private to the implementation; the public header no longer includes stdlib.h transitively through an allocation helper.

Apply the normal SC_ASYNC_EXECUTION_EXPORT convention to out-of-line interfaces. Compile the implementation and consumers with the same execution provider and feature settings. If execution integration is disabled, the implementation translation unit is empty. SC.cpp includes the new implementation for single-translation-unit builds. Amalgamation includes it under its existing implementation guard.

## Confirmation

Build and run the dedicated tests and normal/failure demos in Debug and Release, including VM builds where available. Check dependency metadata and regenerate/build regular and standalone single-file libraries. Verify enabled-provider amalgamation as well as the default disabled-provider configuration. This is a structural compile-cost reduction, not a measured build-time speedup claim.
