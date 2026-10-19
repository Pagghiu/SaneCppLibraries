# ASYNCEXECUTION-0015 - One-Shot File Readiness

Status: Accepted
Date: 2026-10-19

## Context

Async, Await and AsyncFibers expose readable file readiness separately from reading bytes. AsyncExecution needs the same borrowed operation for event sources whose data must be consumed by another API. A readiness indication is not a byte transfer, EOF observation or ongoing subscription.

## Decision

Add AsyncExecutionContext::filePoll(const FileDescriptor&) returning a lazy sender with no success values. It borrows the descriptor through terminal quiescence and starts one AsyncFileReadiness request. Success observes readable readiness without consuming data. Repeating observation requires another sender; no hidden rearming or polling loop is added.

Preserve native unsupported/backend errors. In particular, valid Windows handles report AsyncError::OperationUnsupported because generic IOCP handle readiness is not implemented. Do not emulate readiness with timers or worker blocking. Invalid descriptors report AsyncError::InvalidFileHandle before submission; prestop follows the common sender contract and prevents validation/native submission.

Use the existing immovable request, owner dispatcher, stop registration and cancellation join. A terminal receiver may release connected storage. Non-template factory/submission code belongs in AsyncExecution.cpp; no new dependency, scheduler or allocation policy is introduced. The owning runtime accesses this operation through getContext rather than duplicating every borrowing method.

## Confirmation

Exercise errors and prestop on every platform; check the exact Windows unsupported identity for a valid pipe. On supported backends join a delayed pipe producer with readiness, observe readiness twice without consuming bytes, then read the original byte. Cancel an active wait from another thread, free its connected storage at stopped delivery, write afterwards and reuse the descriptor for another wait. Release connected storage from value/error callbacks too. Run supported Linux backends explicitly, Debug/Release and available platform checks; preserve header/implementation, SC.cpp and amalgamation boundaries.
